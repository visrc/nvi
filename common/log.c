/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: log.c,v 5.1 1992/11/11 09:29:04 bostic Exp $ (Berkeley) $Date: 1992/11/11 09:29:04 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exf.h"
#include "/usr/src/lib/libc/db/btree/btree.h"
#include "log.h"
#include "screen.h"
#include "extern.h"

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

	m.lno = curf->lno;
	m.cno = curf->cno;
	data.data = &m;
	data.size = sizeof(MARK);

	/* If the last record was a cursor move, just overwrite it. */
	if (c_ltype == LOG_CURSOR) {
		key.data = &recno_saved;
		key.size = sizeof(recno_t);
		if (ep->log->put(ep->log, &key, &data, R_SETCURSOR) == -1)
			LOG_ERR;
	} else if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

#define	RRR	((struct BTREE *)(curf->log->internal))->bt_rcursor
TRACE("put into log %u: cursor @ %u\n", RRR, m.lno);

	bcopy(key.data, &recno_saved, sizeof(recno_t));
	
	c_ltype = LOG_CURSOR;
	data.data = &c_ltype;
	data.size = sizeof(u_char);
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;
TRACE("put into log %u: CURSOR LOG\n", RRR);
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

	if (nolog)
		return (0);

	if (action == LINE_DELETE || action == LINE_RESET) {
		if ((data.data = file_gline(ep, lno, &data.size)) == NULL) {
			GETLINE_ERR(lno);
			return (1);
		}
		if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
			LOG_ERR;
TRACE("put into log %u: line %u\n", RRR, lno);
	}

	data.data = &lno;
	data.size = sizeof(recno_t);
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;
TRACE("put into log %u: lno %u\n", RRR, lno);

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

	data.data = &c_ltype;
	data.size = sizeof(u_char);
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
	u_char kch2;

	if (nolog)
		return (0);

	data.data = mp;
	data.size = sizeof(MARK);
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

	kch2 = kch;
	data.data = &kch2;
	data.size = sizeof(u_char);
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;

	c_ltype = LOG_MARK;
	if (ep->log->put(ep->log, &key, &data, R_CURSORLOG) == -1)
		LOG_ERR;
	return (0);
}

/*
 * log_undo --
 *	Undo the last operation.
 */
int
log_undo(ep, rp)
	EXF *ep;
	MARK *rp;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop, rval;
	u_char ltype;

	ltype = LOG_NOTYPE;

	if (nolog) {
		msg("Logging not being performed, undo not possible.");
		return (1);
	}
	nolog = 1;			/* Turn off logging while in undo. */

	/* Rewind to the just logged current cursor record. */
	if (ep->log->seq(ep->log, &key, &data, R_PREV))
		LOG_ERR;

	for (didop = 0;;) {
		if (ep->log->seq(ep->log, &key, &data, R_PREV))
			LOG_ERR;

		ltype = *(u_char *)data.data;
		if (ltype == LOG_START) {
			msg("Nothing to undo.");
			nolog = 0;
			return (1);
		}

		if (ep->log->seq(ep->log, &key, &data, R_PREV))
			LOG_ERR;

		switch(ltype) {
		case LOG_CURSOR:
			if (didop != 0) {
				bcopy(data.data, rp, sizeof(MARK));
				if ((rval = ep->log->seq(ep->log,
				    &key, &data, R_PREV)) && rval != 1)
					LOG_ERR;
				nolog = 0;
				c_ltype = LOG_NOTYPE;
TRACE("undo complete: cursor @ %u\n", RRR);
				return (0);
			}
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			bcopy(data.data, &lno, sizeof(recno_t));
			if (file_dline(ep, lno))
				return (1);
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			bcopy(data.data, &lno, sizeof(recno_t));
			if (ep->log->seq(ep->log, &key, &data, R_PREV))
				LOG_ERR;
			if (file_iline(ep, lno, data.data, data.size))
				return (1);
			break;
		case LOG_LINE_RESET:
			didop = 1;
			bcopy(data.data, &lno, sizeof(recno_t));
			if (ep->log->seq(ep->log, &key, &data, R_PREV))
				LOG_ERR;
			if (file_sline(ep, lno, data.data, data.size))
				return (1);
			break;
		case LOG_MARK:
			didop = 1;
			bcopy(data.data, &m, sizeof(MARK));
			if (ep->log->seq(ep->log, &key, &data, R_PREV))
				LOG_ERR;
			if (mark_set(*(u_char *)data.data, &m))
				return (1);
			break;
		default:
			abort();
		}
	}
	/* NOTREACHED */
}
