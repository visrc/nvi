/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: log.c,v 5.4 1992/12/20 15:13:19 bostic Exp $ (Berkeley) $Date: 1992/12/20 15:13:19 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "exf.h"
#include "log.h"
#include "screen.h"

static u_char c_ltype = LOG_NOTYPE;
static int nolog;

/* Try and restart the log on failure, i.e. if we run out of memory. */
#define	LOG_ERR {							\
	bell();								\
	msg("Error: %s/%d: put log error: %s.",				\
	    tail(__FILE__), __LINE__, strerror(errno));			\
	(void)ep->log->close(ep->log);					\
	if (!log_init(ep))						\
		msg("Log restarted.");					\
	return (1);							\
}

/*
 * log_init --
 *	Initialize the logging package.
 */
int
log_init(ep)
	EXF *ep;
{
	DBT key;

	ep->l_lp = NULL;
	ep->l_len = 0;

	ep->log = dbopen(NULL, O_CREAT | O_EXLOCK | O_NONBLOCK | O_RDWR,
	    S_IRUSR | S_IWUSR, DB_RECNO, NULL);
	if (ep->log == NULL) {
		bell();
		msg("log db: %s", strerror(errno));
		nolog = 1;
		return (1);
	}

	/*
	 * There's a problem with the recno package in that when you do an
	 * R_CURSORLOG with no records in the package, you get record number
	 * 1.  If you then rewind (R_PREV) to SOF and do a R_CURSORLOG, you
	 * get record number 2.  This is bad.  Insert a dummy record just
	 * to keep things straight.
	 */
	c_ltype = LOG_START;
	key.data = &c_ltype;
	key.size = sizeof(u_char);
	if (ep->log->put(ep->log, &key, &key, R_CURSORLOG) == -1) {
		msg("Error: %s/%d: put log error: %s.",
		    tail(__FILE__), __LINE__, strerror(errno));
		(void)(ep->log->close)(ep->log);
		nolog = 1;
		return (1);
	}

	return (0);
}

/*
 * log_end --
 *	Close the logging package.
 */
int
log_end(ep)
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
log_cursor(ep)
	EXF *ep;
{
	static recno_t recno_saved;
	DBT data, key;
	MARK m;

	if (nolog)
		return (0);

	BINC(ep->l_lp, ep->l_len, sizeof(u_char) + sizeof(MARK));
		
	ep->l_lp[0] = LOG_CURSOR;

	m.lno = ep->lno;
	m.cno = ep->cno;
	bcopy(&m, ep->l_lp + sizeof(u_char), sizeof(MARK));

	data.data = ep->l_lp;
	data.size = sizeof(u_char) + sizeof(MARK);

	/* If the last record was a cursor move, just overwrite it. */
	if (c_ltype == LOG_CURSOR) {
		key.data = &recno_saved;
		key.size = sizeof(recno_t);
		if (ep->log->put(ep->log, &key, &data, R_SETCURSOR) == -1)
			LOG_ERR;
	} else if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

	bcopy(key.data, &recno_saved, sizeof(recno_t));
	c_ltype = LOG_CURSOR;
	return (0);
}

/*
 * log_line --
 *	Log a line change.
 */
int
log_line(ep, lno, action)
	EXF *ep;
	recno_t lno;
	u_int action;
{
	DBT data, key;
	size_t len;
	u_char *lp;

	if (nolog)
		return (0);

	FF_CLR(ep, F_UNDO);	/* Vi hack.  Clear the undo flag. */

	if ((lp = file_gline(ep, lno, &len)) == NULL) {
		GETLINE_ERR(lno);
		return (1);
	}
	BINC(ep->l_lp, ep->l_len, len + sizeof(u_char) + sizeof(recno_t));

	switch(action) {
	case LINE_APPEND:
		c_ltype = LOG_LINE_APPEND;
		break;
	case LINE_DELETE:
		c_ltype = LOG_LINE_DELETE;
		break;
	case LINE_INSERT:
		c_ltype = LOG_LINE_INSERT;
		break;
	case LINE_RESET:
		c_ltype = LOG_LINE_RESET;
		break;
	default:
		abort();
	}

	ep->l_lp[0] = c_ltype;
	bcopy(&lno, ep->l_lp + sizeof(u_char), sizeof(recno_t));
	bcopy(lp, ep->l_lp + sizeof(u_char) + sizeof(recno_t), len);

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
log_mark(ep, kch, mp)
	EXF *ep;
	u_int kch;
	MARK *mp;
{
	DBT data, key;

	if (nolog)
		return (0);

	BINC(ep->l_lp,
	    ep->l_len, sizeof(u_char) + sizeof(u_char) + sizeof(MARK));

	ep->l_lp[0] = LOG_MARK;
	ep->l_lp[1] = kch;
	bcopy(mp, ep->l_lp + sizeof(u_char) + sizeof(u_char), sizeof(MARK));

	data.data = ep->l_lp;
	data.size = sizeof(u_char) + sizeof(u_char) + sizeof(MARK);
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

	c_ltype = LOG_MARK;
	return (0);
}

/*
 * Log_backward --
 *	Roll the log backward one operation.
 */
int
log_backward(ep, rp, undolno)
	EXF *ep;
	MARK *rp;
	recno_t undolno;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop;

	if (nolog) {
		msg("Logging not being performed, undo not possible.");
		return (1);
	}
	nolog = 1;			/* Turn off logging. */
	c_ltype = LOG_NOTYPE;		/* Turn off caching. */

	for (didop = 0;;) {
		if (ep->log->seq(ep->log, &key, &data, R_PREV))
			LOG_ERR;

		switch(*(u_char *)data.data) {
		case LOG_CURSOR:
			if (didop != 0 || undolno != OOBLNO) {
				bcopy(data.data +
				    sizeof(u_char), rp, sizeof(MARK));
				if (undolno != OOBLNO && rp->lno == lno)
					break;
				if (ep->log->seq(ep->log, &key, &data, R_PREV))
					LOG_ERR;
				nolog = 0;
				return (0);
			}
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			bcopy(data.data +
			    sizeof(u_char), &lno, sizeof(recno_t));
			if (file_dline(ep, lno))
				goto err;
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			bcopy(data.data +
			    sizeof(u_char), &lno, sizeof(recno_t));
			if (file_iline(ep, lno, data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			break;
		case LOG_LINE_RESET:
			didop = 1;
			if (ep->log->seq(ep->log, &key, &data, R_PREV))
				LOG_ERR;
			bcopy(data.data +
			    sizeof(u_char), &lno, sizeof(recno_t));
			if (file_sline(ep, lno, data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			break;
		case LOG_MARK:
			didop = 1;
			bcopy(data.data +
			    sizeof(u_char) + sizeof(u_char), &m, sizeof(MARK));
			if (mark_set(*(((u_char *)data.data) + 1), &m))
				goto err;
			break;
		case LOG_START:
			if (didop == 0)
				msg("Nothing to undo.");
err:			nolog = 0;
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
log_forward(ep, rp)
	EXF *ep;
	MARK *rp;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop, rval;

	if (nolog) {
		msg("Logging not being performed, roll-forward not possible.");
		return (1);
	}
	nolog = 1;			/* Turn off logging. */
	c_ltype = LOG_NOTYPE;		/* Turn off caching. */

	for (didop = 0;;) {
		rval = ep->log->seq(ep->log, &key, &data, R_NEXT);
		if (rval == 1) {
			msg("No further changes to roll forward.");
			nolog = 0;
			return (1);
		}
		if (rval)
			LOG_ERR;

		switch(*(u_char *)data.data) {
		case LOG_CURSOR:
			if (didop != 0) {
				bcopy(data.data +
				    sizeof(u_char), rp, sizeof(MARK));
				nolog = 0;
				return (0);
			}
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			bcopy(data.data +
			    sizeof(u_char), &lno, sizeof(recno_t));
			if (file_iline(ep, lno, data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t))) {
				nolog = 0;
				return (1);
			}
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			bcopy(data.data +
			    sizeof(u_char), &lno, sizeof(recno_t));
			if (file_dline(ep, lno)) {
				nolog = 0;
				return (1);
			}
			break;
		case LOG_LINE_RESET:
			didop = 1;
			if (ep->log->seq(ep->log, &key, &data, R_NEXT))
				LOG_ERR;
			bcopy(data.data +
			    sizeof(u_char), &lno, sizeof(recno_t));
			if (file_sline(ep, lno, data.data + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				return (1);
			break;
		case LOG_MARK:
			didop = 1;
			bcopy(data.data +
			    sizeof(u_char) + sizeof(u_char), &m, sizeof(MARK));
			if (mark_set(*(((u_char *)data.data) + 1), &m)) {
				nolog = 0;
				return (1);
			}
			break;
		default:
			abort();
		}
	}
	/* NOTREACHED */
}
