/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: db.c,v 10.28 2000/07/15 20:26:33 skimo Exp $ (Berkeley) $Date: 2000/07/15 20:26:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "../vi/vi.h"

static int scr_update __P((SCR *, db_recno_t, lnop_t, int));
static int append __P((SCR*, db_recno_t, CHAR_T*, size_t));

/*
 * db_eget --
 *	Front-end to db_get, special case handling for empty files.
 *
 * PUBLIC: int db_eget __P((SCR *, db_recno_t, CHAR_T **, size_t *, int *));
 */
int
db_eget(sp, lno, pp, lenp, isemptyp)
	SCR *sp;
	db_recno_t lno;				/* Line number. */
	CHAR_T **pp;				/* Pointer store. */
	size_t *lenp;				/* Length store. */
	int *isemptyp;
{
	db_recno_t l1;

	if (isemptyp != NULL)
		*isemptyp = 0;

	/* If the line exists, simply return it. */
	if (!db_get(sp, lno, 0, pp, lenp))
		return (0);

	/*
	 * If the user asked for line 0 or line 1, i.e. the only possible
	 * line in an empty file, find the last line of the file; db_last
	 * fails loudly.
	 */
	if ((lno == 0 || lno == 1) && db_last(sp, &l1))
		return (1);

	/* If the file isn't empty, fail loudly. */
	if ((lno != 0 && lno != 1) || l1 != 0) {
		db_err(sp, lno);
		return (1);
	}

	if (isemptyp != NULL)
		*isemptyp = 1;

	return (1);
}

/*
 * db_get --
 *	Look in the text buffers for a line, followed by the cache, followed
 *	by the database.
 *
 * PUBLIC: int db_get __P((SCR *, db_recno_t, u_int32_t, CHAR_T **, size_t *));
 */
int
db_get(sp, lno, flags, pp, lenp)
	SCR *sp;
	db_recno_t lno;				/* Line number. */
	u_int32_t flags;
	CHAR_T **pp;				/* Pointer store. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;
	EXF *ep;
	TEXT *tp;
	db_recno_t l1, l2;
	CHAR_T *wp;
	size_t wlen;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but
	 * have to have an OOB condition for the look-aside into the input
	 * buffer anyway.
	 */
	if (lno == 0)
		goto err1;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		goto err3;
	}

	if (LF_ISSET(DBG_NOCACHE))
		goto nocache;

	/*
	 * Look-aside into the TEXT buffers and see if the line we want
	 * is there.
	 */
	if (F_ISSET(sp, SC_TINPUT)) {
		l1 = ((TEXT *)sp->tiq.cqh_first)->lno;
		l2 = ((TEXT *)sp->tiq.cqh_last)->lno;
		if (l1 <= lno && l2 >= lno) {
#if defined(DEBUG) && 0
			vtrace(sp,
			    "retrieve TEXT buffer line %lu\n", (u_long)lno);
#endif
			for (tp = sp->tiq.cqh_first;
			    tp->lno != lno; tp = tp->q.cqe_next);
			if (lenp != NULL)
				*lenp = tp->len;
			if (pp != NULL)
				*pp = tp->lb;
			return (0);
		}
		/*
		 * Adjust the line number for the number of lines used
		 * by the text input buffers.
		 */
		if (lno > l2)
			lno -= l2 - l1;
	}

	/* Look-aside into the cache, and see if the line we want is there. */
	if (lno == ep->c_lno) {
#if defined(DEBUG) && 0
		vtrace(sp, "retrieve cached line %lu\n", (u_long)lno);
#endif
		if (lenp != NULL)
			*lenp = ep->c_len;
		if (pp != NULL)
			*pp = ep->c_lp;
		return (0);
	}
	ep->c_lno = OOBLNO;

nocache:
	/* Get the line from the underlying database. */
	memset(&key, 0, sizeof(key));
	key.data = &lno;
	key.size = sizeof(lno);
	memset(&data, 0, sizeof(data));
	switch (ep->db->get(ep->db, NULL, &key, &data, 0)) {
        default:
		goto err2;
	case DB_NOTFOUND:
err1:		if (LF_ISSET(DBG_FATAL))
err2:			db_err(sp, lno);
alloc_err:
err3:		if (lenp != NULL)
			*lenp = 0;
		if (pp != NULL)
			*pp = NULL;
		return (1);
	case 0:
		;
	}

	FILE2INT(sp, data.data, data.size, wp, wlen);

	/* Reset the cache. */
	BINC_GOTOW(sp, ep->c_lp, ep->c_blen, wlen);
	MEMCPYW(ep->c_lp, wp, wlen);
	ep->c_lno = lno;
	ep->c_len = wlen;

#if defined(DEBUG) && 0
	vtrace(sp, "retrieve DB line %lu\n", (u_long)lno);
#endif
	if (lenp != NULL)
		*lenp = wlen;
	if (pp != NULL)
		*pp = ep->c_lp;
	return (0);
}

/*
 * db_delete --
 *	Delete a line from the file.
 *
 * PUBLIC: int db_delete __P((SCR *, db_recno_t));
 */
int
db_delete(sp, lno)
	SCR *sp;
	db_recno_t lno;
{
	DBT key;
	EXF *ep;

#if defined(DEBUG) && 0
	vtrace(sp, "delete line %lu\n", (u_long)lno);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Update marks, @ and global commands. */
	if (mark_insdel(sp, LINE_DELETE, lno))
		return (1);
	if (ex_g_insdel(sp, LINE_DELETE, lno))
		return (1);

	/* Log change. */
	log_line(sp, lno, LOG_LINE_DELETE);

	/* Update file. */
	memset(&key, 0, sizeof(key));
	key.data = &lno;
	key.size = sizeof(lno);
	if ((sp->db_error = ep->db->del(ep->db, NULL, &key, 0)) != 0) {
		msgq(sp, M_DBERR, "003|unable to delete line %lu", 
		    (u_long)lno);
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno <= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		--ep->c_nlines;

	/* File now modified. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_DELETE, 1));
}

/* maybe this could be simpler
 *
 * DB3 behaves differently from DB1
 *
 * if lno != 0 just go to lno and put the new line after it
 * if lno == 0 then if there are any record, put in front of the first
 *		    otherwise just append to the end thus creating the first
 *				line
 */
static int
append(sp, lno, p, len)
	SCR *sp;
	db_recno_t lno;
	CHAR_T *p;
	size_t len;
{
	DBT data, key;
	DBC *dbcp_put;
	EXF *ep;
	char *fp;
	size_t flen;

	ep = sp->ep;

	memset(&key, 0, sizeof(key));
	key.data = &lno;
	key.size = sizeof(lno);
	memset(&data, 0, sizeof(data));

	if ((sp->db_error = ep->db->cursor(ep->db, NULL, &dbcp_put, 0)) != 0)
	    return 1;

	INT2FILE(sp, p, len, fp, flen);

	if (lno != 0) {
	    if ((sp->db_error = dbcp_put->c_get(dbcp_put, &key, &data, DB_SET)) != 0) 
		goto err2;

	    data.data = fp;
	    data.size = flen;
	    if ((sp->db_error = dbcp_put->c_put(dbcp_put, &key, &data, DB_AFTER)) != 0) {
err2:
		(void)dbcp_put->c_close(dbcp_put);
		return (1);
	    }
	} else {
	    if ((sp->db_error = dbcp_put->c_get(dbcp_put, &key, &data, DB_FIRST)) != 0) {
		if (sp->db_error != DB_NOTFOUND)
		    goto err2;

		data.data = fp;
		data.size = flen;
		if ((sp->db_error = ep->db->put(ep->db, NULL, &key, &data, DB_APPEND)) != 0) {
		    goto err2;
		}
	    } else {
		key.data = &lno;
		key.size = sizeof(lno);
		data.data = fp;
		data.size = flen;
		if ((sp->db_error = dbcp_put->c_put(dbcp_put, &key, &data, DB_BEFORE)) != 0) {
		    goto err2;
		}
	    }
	}

	(void)dbcp_put->c_close(dbcp_put);

	return 0;
}

/*
 * db_append --
 *	Append a line into the file.
 *
 * PUBLIC: int db_append __P((SCR *, int, db_recno_t, CHAR_T *, size_t));
 */
int
db_append(sp, update, lno, p, len)
	SCR *sp;
	int update;
	db_recno_t lno;
	CHAR_T *p;
	size_t len;
{
	EXF *ep;
	int rval;

#if defined(DEBUG) && 0
	vtrace(sp, "append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Update file. */
	if (append(sp, lno, p, len) != 0) {
		msgq(sp, M_DBERR, "004|unable to append to line %lu", 
			(u_long)lno);
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno < ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, lno + 1, LOG_LINE_APPEND);

	/* Update marks, @ and global commands. */
	rval = 0;
	if (mark_insdel(sp, LINE_INSERT, lno + 1))
		rval = 1;
	if (ex_g_insdel(sp, LINE_INSERT, lno + 1))
		rval = 1;

	/*
	 * Update screen.
	 *
	 * XXX
	 * Nasty hack.  If multiple lines are input by the user, they aren't
	 * committed until an <ESC> is entered.  The problem is the screen was
	 * updated/scrolled as each line was entered.  So, when this routine
	 * is called to copy the new lines from the cut buffer into the file,
	 * it has to know not to update the screen again.
	 */
	return (scr_update(sp, lno, LINE_APPEND, update) || rval);
}

/*
 * db_insert --
 *	Insert a line into the file.
 *
 * PUBLIC: int db_insert __P((SCR *, db_recno_t, CHAR_T *, size_t));
 */
int
db_insert(sp, lno, p, len)
	SCR *sp;
	db_recno_t lno;
	CHAR_T *p;
	size_t len;
{
	DBT data, key;
	DBC *dbcp_put;
	EXF *ep;
	int rval;

#if defined(DEBUG) && 0
	vtrace(sp, "insert before %lu: len %lu {%.*s}\n",
	    (u_long)lno, (u_long)len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Update file. */
	if (append(sp, lno - 1, p, len) != 0) {
		msgq(sp, M_DBERR, "005|unable to insert at line %lu", 
			(u_long)lno);
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno >= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, lno, LOG_LINE_INSERT);

	/* Update marks, @ and global commands. */
	rval = 0;
	if (mark_insdel(sp, LINE_INSERT, lno))
		rval = 1;
	if (ex_g_insdel(sp, LINE_INSERT, lno))
		rval = 1;

	/* Update screen. */
	return (scr_update(sp, lno, LINE_INSERT, 1) || rval);
}

/*
 * db_set --
 *	Store a line in the file.
 *
 * PUBLIC: int db_set __P((SCR *, db_recno_t, CHAR_T *, size_t));
 */
int
db_set(sp, lno, p, len)
	SCR *sp;
	db_recno_t lno;
	CHAR_T *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;
	char *fp;
	size_t flen;

#if defined(DEBUG) && 0
	vtrace(sp, "replace line %lu: len %lu {%.*s}\n",
	    (u_long)lno, (u_long)len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Log before change. */
	log_line(sp, lno, LOG_LINE_RESET_B);

	INT2FILE(sp, p, len, fp, flen);

	/* Update file. */
	memset(&key, 0, sizeof(key));
	key.data = &lno;
	key.size = sizeof(lno);
	memset(&data, 0, sizeof(data));
	data.data = fp;
	data.size = flen;
	if ((sp->db_error = ep->db->put(ep->db, NULL, &key, &data, 0)) != 0) {
		msgq(sp, M_DBERR, "006|unable to store line %lu", (u_long)lno);
		return (1);
	}

	/* Flush the cache, before logging or screen update. */
	if (lno == ep->c_lno)
		ep->c_lno = OOBLNO;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, lno, LOG_LINE_RESET_F);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_RESET, 1));
}

/*
 * db_exist --
 *	Return if a line exists.
 *
 * PUBLIC: int db_exist __P((SCR *, db_recno_t));
 */
int
db_exist(sp, lno)
	SCR *sp;
	db_recno_t lno;
{
	EXF *ep;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}

	if (lno == OOBLNO)
		return (0);
		
	/*
	 * Check the last-line number cache.  Adjust the cached line
	 * number for the lines used by the text input buffers.
	 */
	if (ep->c_nlines != OOBLNO)
		return (lno <= (F_ISSET(sp, SC_TINPUT) ?
		    ep->c_nlines + (((TEXT *)sp->tiq.cqh_last)->lno -
		    ((TEXT *)sp->tiq.cqh_first)->lno) : ep->c_nlines));

	/* Go get the line. */
	return (!db_get(sp, lno, 0, NULL, NULL));
}

/*
 * db_last --
 *	Return the number of lines in the file.
 *
 * PUBLIC: int db_last __P((SCR *, db_recno_t *));
 */
int
db_last(sp, lnop)
	SCR *sp;
	db_recno_t *lnop;
{
	DBT data, key;
	DBC *dbcp;
	EXF *ep;
	db_recno_t lno;
	CHAR_T *wp;
	size_t wlen;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/*
	 * Check the last-line number cache.  Adjust the cached line
	 * number for the lines used by the text input buffers.
	 */
	if (ep->c_nlines != OOBLNO) {
		*lnop = ep->c_nlines;
		if (F_ISSET(sp, SC_TINPUT))
			*lnop += ((TEXT *)sp->tiq.cqh_last)->lno -
			    ((TEXT *)sp->tiq.cqh_first)->lno;
		return (0);
	}

	memset(&key, 0, sizeof(key));
	key.data = &lno;
	key.size = sizeof(lno);
	memset(&data, 0, sizeof(data));

	if ((sp->db_error = ep->db->cursor(ep->db, NULL, &dbcp, 0)) != 0)
	    goto err1;
	switch (sp->db_error = dbcp->c_get(dbcp, &key, &data, DB_LAST)) {
        case DB_NOTFOUND:
		*lnop = 0;
		return (0);
	default:
		(void)dbcp->c_close(dbcp);
alloc_err:
err1:
		msgq(sp, M_DBERR, "007|unable to get last line");
		*lnop = 0;
		return (1);
        case 0:
		;
	}
	(void)dbcp->c_close(dbcp);

	FILE2INT(sp, data.data, data.size, wp, wlen);

	/* Fill the cache. */
	BINC_GOTOW(sp, ep->c_lp, ep->c_blen, wlen);
	MEMCPYW(ep->c_lp, wp, wlen);
	memcpy(&lno, key.data, sizeof(lno));
	ep->c_nlines = ep->c_lno = lno;
	ep->c_len = wlen;

	/* Return the value. */
	*lnop = (F_ISSET(sp, SC_TINPUT) &&
	    ((TEXT *)sp->tiq.cqh_last)->lno > lno ?
	    ((TEXT *)sp->tiq.cqh_last)->lno : lno);
	return (0);
}

/*
 * db_err --
 *	Report a line error.
 *
 * PUBLIC: void db_err __P((SCR *, db_recno_t));
 */
void
db_err(sp, lno)
	SCR *sp;
	db_recno_t lno;
{
	msgq(sp, M_ERR,
	    "008|Error: unable to retrieve line %lu", (u_long)lno);
}

/*
 * scr_update --
 *	Update all of the screens that are backed by the file that
 *	just changed.
 */
static int
scr_update(sp, lno, op, current)
	SCR *sp;
	db_recno_t lno;
	lnop_t op;
	int current;
{
	EXF *ep;
	SCR *tsp;
	WIN *wp;

	if (F_ISSET(sp, SC_EX))
		return (0);

	/* XXXX goes outside of window */
	ep = sp->ep;
	if (ep->refcnt != 1)
		for (wp = sp->gp->dq.cqh_first; wp != (void *)&sp->gp->dq; 
		    wp = wp->q.cqe_next)
			for (tsp = wp->scrq.cqh_first;
			    tsp != (void *)&wp->scrq; tsp = tsp->q.cqe_next)
			if (sp != tsp && tsp->ep == ep)
				if (vs_change(tsp, lno, op))
					return (1);
	return (current ? vs_change(sp, lno, op) : 0);
}
