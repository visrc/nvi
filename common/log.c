/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: log.c,v 5.12 1993/04/05 07:12:34 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:12:34 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

/* Try and restart the log on failure, i.e. if we run out of memory. */
#define	LOG_ERR {							\
	msgq(sp, M_ERROR, "Error: %s/%d: put log error: %s.",		\
	    tail(__FILE__), __LINE__, strerror(errno));			\
	(void)ep->log->close(ep->log);					\
	if (!log_init(sp, ep))						\
		msgq(sp, M_DISPLAY, "Log restarted.");			\
	return (1);							\
}

/*
 * log_init --
 *	Initialize the logging package.
 */
int
log_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	DBT key;

	ep->l_lp = NULL;
	ep->l_len = 0;

	ep->log = dbopen(NULL, O_CREAT | O_EXLOCK | O_NONBLOCK | O_RDWR,
	    S_IRUSR | S_IWUSR, DB_RECNO, NULL);
	if (ep->log == NULL) {
		msgq(sp, M_ERROR, "log db: %s", strerror(errno));
		F_SET(ep, F_NOLOG);
		return (1);
	}

	/*
	 * There's a problem with the recno package in that when you do an
	 * R_CURSORLOG with no records in the package, you get record number
	 * 1.  If you then rewind (R_PREV) to SOF and do a R_CURSORLOG, you
	 * get record number 2.  This is bad.  Insert a dummy record just
	 * to keep things straight.
	 */
	ep->l_ltype = LOG_START;
	key.data = &ep->l_ltype;
	key.size = sizeof(u_char);
	if (ep->log->put(ep->log, &key, &key, R_CURSORLOG) == -1) {
		msgq(sp, M_ERROR, "Error: %s/%d: put log error: %s.",
		    tail(__FILE__), __LINE__, strerror(errno));
		(void)(ep->log->close)(ep->log);
		F_SET(ep, F_NOLOG);
		return (1);
	}

	return (0);
}

/*
 * log_end --
 *	Close the logging package.
 */
int
log_end(sp, ep)
	SCR *sp;
	EXF *ep;
{
	if (ep->log != NULL) {
		(void)(ep->log->close)(ep->log);
		ep->log = NULL;
	}
	if (ep->l_lp != NULL) {
		free(ep->l_lp);
		ep->l_lp = NULL;
	}
	ep->l_len = 0;
	return (0);
}
		
/*
 * log_cursor --
 *	Log a cursor position.
 */
int
log_cursor(sp, ep)
	SCR *sp;
	EXF *ep;
{
	DBT data, key;
	MARK m;

	if (F_ISSET(ep, F_NOLOG))
		return (0);

	BINC(sp, ep->l_lp, ep->l_len, sizeof(u_char) + sizeof(MARK));
		
	ep->l_lp[0] = LOG_CURSOR;

	m.lno = sp->lno;
	m.cno = sp->cno;
	memmove(ep->l_lp + sizeof(u_char), &m, sizeof(MARK));

	data.data = ep->l_lp;
	data.size = sizeof(u_char) + sizeof(MARK);

	/* If the last record was a cursor move, just overwrite it. */
	if (ep->l_ltype == LOG_CURSOR) {
		key.data = &ep->l_srecno;
		key.size = sizeof(recno_t);
		if (ep->log->put(ep->log, &key, &data, R_SETCURSOR) == -1)
			LOG_ERR;
	} else if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

	memmove(&ep->l_srecno, key.data, sizeof(recno_t));
	ep->l_ltype = LOG_CURSOR;
	return (0);
}

/*
 * log_line --
 *	Log a line change.
 */
int
log_line(sp, ep, lno, action)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	u_int action;
{
	DBT data, key;
	size_t len;
	char *lp;

	if (F_ISSET(ep, F_NOLOG))
		return (0);

	F_CLR(ep, F_UNDO);	/* Vi hack.  Clear the undo flag. */

	if ((lp = file_gline(sp, ep, lno, &len)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}
	BINC(sp, ep->l_lp, ep->l_len, len + sizeof(u_char) + sizeof(recno_t));

	switch(action) {
	case LINE_APPEND:
		ep->l_ltype = LOG_LINE_APPEND;
		break;
	case LINE_DELETE:
		ep->l_ltype = LOG_LINE_DELETE;
		break;
	case LINE_INSERT:
		ep->l_ltype = LOG_LINE_INSERT;
		break;
	case LINE_RESET:
		ep->l_ltype = LOG_LINE_RESET;
		break;
	default:
		abort();
	}

	ep->l_lp[0] = ep->l_ltype;
	memmove(ep->l_lp + sizeof(u_char), &lno, sizeof(recno_t));
	memmove(ep->l_lp + sizeof(u_char) + sizeof(recno_t), lp, len);

	data.data = ep->l_lp;
	data.size = len + sizeof(u_char) + sizeof(recno_t);

	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;
	return (0);
}

/*
 * log_mark --
 *	Log a mark position.
 */
int
log_mark(sp, ep, kch, mp)
	SCR *sp;
	EXF *ep;
	u_int kch;
	MARK *mp;
{
	DBT data, key;

	if (F_ISSET(ep, F_NOLOG))
		return (0);

	BINC(sp, ep->l_lp,
	    ep->l_len, sizeof(u_char) + sizeof(u_char) + sizeof(MARK));

	ep->l_lp[0] = LOG_MARK;
	ep->l_lp[1] = kch;
	memmove(ep->l_lp + sizeof(u_char) + sizeof(u_char), mp, sizeof(MARK));

	data.data = ep->l_lp;
	data.size = sizeof(u_char) + sizeof(u_char) + sizeof(MARK);
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

	ep->l_ltype = LOG_MARK;
	return (0);
}

/*
 * Log_backward --
 *	Roll the log backward one operation.
 */
int
log_backward(sp, ep, rp, undolno)
	SCR *sp;
	EXF *ep;
	MARK *rp;
	recno_t undolno;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop;

	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_DISPLAY,
		    "Logging not being performed, undo not possible.");
		return (1);
	}
	F_SET(ep, F_NOLOG);		/* Turn off logging. */
	ep->l_ltype = LOG_NOTYPE;	/* Turn off caching. */

	for (didop = 0;;) {
		if (ep->log->seq(ep->log, &key, &data, R_PREV))
			LOG_ERR;

		switch(*(u_char *)data.data) {
		case LOG_CURSOR:
			if (didop != 0 || undolno != OOBLNO) {
				memmove(rp, (u_char *)data.data +
				    sizeof(u_char), sizeof(MARK));
				if (undolno != OOBLNO && rp->lno == lno)
					break;
				if (ep->log->seq(ep->log, &key, &data, R_PREV))
					LOG_ERR;
				F_CLR(ep, F_NOLOG);
				return (0);
			}
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			memmove(&lno, (u_char *)data.data +
			    sizeof(u_char), sizeof(recno_t));
			if (file_dline(sp, ep, lno))
				goto err;
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			memmove(&lno, (u_char *)data.data +
			    sizeof(u_char), sizeof(recno_t));
			if (file_iline(sp, ep,
			    lno, (char *)data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			break;
		case LOG_LINE_RESET:
			didop = 1;
			if (ep->log->seq(ep->log, &key, &data, R_PREV))
				LOG_ERR;
			memmove(&lno, (u_char *)data.data +
			    sizeof(u_char), sizeof(recno_t));
			if (file_sline(sp, ep,
			    lno, (char *)data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			break;
		case LOG_MARK:
			didop = 1;
			memmove(&m, (u_char *)data.data +
			    sizeof(u_char) + sizeof(u_char), sizeof(MARK));
			if (mark_set(sp, ep, *(((u_char *)data.data) + 1), &m))
				goto err;
			break;
		case LOG_START:
			if (didop == 0)
				msgq(sp, M_DISPLAY, "Nothing to undo.");
err:			F_CLR(ep, F_NOLOG);
			return (1);
		default:
			abort();
		}
	}
	/* NOTREACHED */
}

/*
 * Log_forward --
 *	Roll the log forward one operation.
 */
int
log_forward(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop, rval;

	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_DISPLAY,
		    "Logging not being performed, roll-forward not possible.");
		return (1);
	}
	F_SET(ep, F_NOLOG);		/* Turn off logging. */
	ep->l_ltype = LOG_NOTYPE;	/* Turn off caching. */

	for (didop = 0;;) {
		rval = ep->log->seq(ep->log, &key, &data, R_NEXT);
		if (rval == 1) {
			msgq(sp, M_DISPLAY,
			    "No further changes to roll forward.");
			F_CLR(ep, F_NOLOG);
			return (1);
		}
		if (rval)
			LOG_ERR;

		switch(*(u_char *)data.data) {
		case LOG_CURSOR:
			if (didop != 0) {
				memmove(rp, (u_char *)data.data +
				    sizeof(u_char), sizeof(MARK));
				F_CLR(ep, F_NOLOG);
				return (0);
			}
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			memmove(&lno, (u_char *)data.data +
			    sizeof(u_char), sizeof(recno_t));
			if (file_iline(sp, ep,
			    lno, (char *)data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t))) {
				F_CLR(ep, F_NOLOG);
				return (1);
			}
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			memmove(&lno, (u_char *)data.data +
			    sizeof(u_char), sizeof(recno_t));
			if (file_dline(sp, ep, lno)) {
				F_CLR(ep, F_NOLOG);
				return (1);
			}
			break;
		case LOG_LINE_RESET:
			didop = 1;
			if (ep->log->seq(ep->log, &key, &data, R_NEXT))
				LOG_ERR;
			memmove(&lno, (u_char *)data.data +
			    sizeof(u_char), sizeof(recno_t));
			if (file_sline(sp, ep,
			    lno, (char *)data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				return (1);
			break;
		case LOG_MARK:
			didop = 1;
			memmove(&m, (u_char *)data.data +
			    sizeof(u_char) + sizeof(u_char), sizeof(MARK));
			if (mark_set(sp, ep,
			    *(((u_char *)data.data) + 1), &m)) {
				F_CLR(ep, F_NOLOG);
				return (1);
			}
			break;
		default:
			abort();
		}
	}
	/* NOTREACHED */
}
