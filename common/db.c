/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: db.c,v 10.9 1995/09/25 11:58:40 bostic Exp $ (Berkeley) $Date: 1995/09/25 11:58:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "../vi/vi.h"

static __inline int scr_update __P((SCR *, recno_t, lnop_t, int));

/*
 * file_gline --
 *	Look in the text buffers for a line; if it's not there
 *	call file_rline to retrieve it from the database.
 *
 * PUBLIC: char *file_gline __P((SCR *, recno_t, size_t *));
 */
char *
file_gline(sp, lno, lenp)
	SCR *sp;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	TEXT *tp;
	recno_t l1, l2;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but
	 * have to have an oob condition for the look-aside into the input
	 * buffer anyway.
	 */
	if (lno == 0)
		return (NULL);

	/*
	 * Look-aside into the TEXT buffers and see if the line we want
	 * is there.
	 */
	if (F_ISSET(sp, S_INPUT)) {
		l1 = ((TEXT *)sp->tiq.cqh_first)->lno;
		l2 = ((TEXT *)sp->tiq.cqh_last)->lno;
		if (l1 <= lno && l2 >= lno) {
			for (tp = sp->tiq.cqh_first;
			    tp->lno != lno; tp = tp->q.cqe_next);
			if (lenp != NULL)
				*lenp = tp->len;
			return (tp->lb);
		}
		/*
		 * Adjust the line number for the number of lines used
		 * by the text input buffers.
		 */
		if (lno > l2)
			lno -= l2 - l1;
	}
	return (file_rline(sp, lno, lenp));
}

/*
 * file_rline --
 *	Look in the cache for a line; if it's not there retrieve
 *	it from the file.
 *
 * PUBLIC: char *file_rline __P((SCR *, recno_t, size_t *));
 */
char *
file_rline(sp, lno, lenp)
	SCR *sp;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;
	EXF *ep;

	/* Check the cache. */
	ep = sp->ep;
	if (lno == ep->c_lno) {
#if defined(DEBUG) && 0
	TRACE(sp, "get cached line %lu\n", lno);
#endif
		if (lenp != NULL)
			*lenp = ep->c_len;
		return (ep->c_lp);
	}
	ep->c_lno = OOBLNO;

	/* Get the line from the underlying database. */
#if defined(DEBUG) && 0
	TRACE(sp, "get uncached line %lu\n", lno);
#endif
	key.data = &lno;
	key.size = sizeof(lno);
	switch (ep->db->get(ep->db, &key, &data, 0)) {
        case -1:
		msgq(sp, M_SYSERR, "002|%s/%d: unable to get line %u",
		    tail(__FILE__), __LINE__, lno);
		/* FALLTHROUGH */
        case 1:
		return (NULL);
		/* NOTREACHED */
	}
	if (lenp != NULL)
		*lenp = data.size;

	/* Fill the cache. */
	ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;

	return (data.data);
}

/*
 * file_dline --
 *	Delete a line from the file.
 *
 * PUBLIC: int file_dline __P((SCR *, recno_t));
 */
int
file_dline(sp, lno)
	SCR *sp;
	recno_t lno;
{
	DBT key;
	EXF *ep;

#if defined(DEBUG) && 0
	TRACE(sp, "delete line %lu\n", lno);
#endif
	/* Update marks and @ and global commands. */
	mark_insdel(sp, LINE_DELETE, lno);
	ex_g_insdel(sp, LINE_DELETE, lno);

	/* Log change. */
	log_line(sp, lno, LOG_LINE_DELETE);

	/* Update file. */
	ep = sp->ep;
	key.data = &lno;
	key.size = sizeof(lno);
	SIGBLOCK;
	if (ep->db->del(ep->db, &key, 0) == 1) {
		msgq(sp, M_SYSERR, "003|%s/%d: unable to delete line %u",
		    tail(__FILE__), __LINE__, lno);
		return (1);
	}
	SIGUNBLOCK;

	/* Flush the cache, update line count, before screen update. */
	if (lno <= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		--ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_DELETE, 1));
}

/*
 * file_aline --
 *	Append a line into the file.
 *
 * PUBLIC: int file_aline __P((SCR *, int, recno_t, char *, size_t));
 */
int
file_aline(sp, update, lno, p, len)
	SCR *sp;
	int update;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;

#if defined(DEBUG) && 0
	TRACE(sp, "append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Update file. */
	ep = sp->ep;
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	SIGBLOCK;
	if (ep->db->put(ep->db, &key, &data, R_IAFTER) == -1) {
		msgq(sp, M_SYSERR, "004|%s/%d: unable to append to line %u",
		    tail(__FILE__), __LINE__, lno);
		return (1);
	}
	SIGUNBLOCK;

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

	/* Update marks and @ and global commands. */
	mark_insdel(sp, LINE_INSERT, lno + 1);
	ex_g_insdel(sp, LINE_INSERT, lno + 1);

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
	return (scr_update(sp, lno, LINE_APPEND, update));
}

/*
 * file_iline --
 *	Insert a line into the file.
 *
 * PUBLIC: int file_iline __P((SCR *, recno_t, char *, size_t));
 */
int
file_iline(sp, lno, p, len)
	SCR *sp;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;

#if defined(DEBUG) && 0
	TRACE(sp,
	    "insert before %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif

	/* Update file. */
	ep = sp->ep;
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	SIGBLOCK;
	if (ep->db->put(ep->db, &key, &data, R_IBEFORE) == -1) {
		msgq(sp, M_SYSERR, "005|%s/%d: unable to insert at line %u",
		    tail(__FILE__), __LINE__, lno);
		return (1);
	}
	SIGUNBLOCK;

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

	/* Update marks and @ and global commands. */
	mark_insdel(sp, LINE_INSERT, lno);
	ex_g_insdel(sp, LINE_INSERT, lno);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_INSERT, 1));
}

/*
 * file_sline --
 *	Store a line in the file.
 *
 * PUBLIC: int file_sline __P((SCR *, recno_t, char *, size_t));
 */
int
file_sline(sp, lno, p, len)
	SCR *sp;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;

#if defined(DEBUG) && 0
	TRACE(sp,
	    "replace line %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Log before change. */
	log_line(sp, lno, LOG_LINE_RESET_B);

	/* Update file. */
	ep = sp->ep;
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	SIGBLOCK;
	if (ep->db->put(ep->db, &key, &data, 0) == -1) {
		msgq(sp, M_SYSERR, "006|%s/%d: unable to store line %u",
		    tail(__FILE__), __LINE__, lno);
		return (1);
	}
	SIGUNBLOCK;

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
 * file_eline --
 *	Return if a line exists.
 *
 * PUBLIC: int file_eline __P((SCR *, recno_t));
 */
int
file_eline(sp, lno)
	SCR *sp;
	recno_t lno;
{
	EXF *ep;

	/* Check the cache. */
	ep = sp->ep;
	if (ep->c_nlines != OOBLNO)
		return (lno > OOBLNO && (lno <= (F_ISSET(sp, S_INPUT) &&
		    ((TEXT *)sp->tiq.cqh_last)->lno > ep->c_nlines ?
		    ((TEXT *)sp->tiq.cqh_last)->lno : ep->c_nlines)));

	/* Go get the line. */
	return (file_gline(sp, lno, NULL) != NULL);
}

/*
 * file_lline --
 *	Return the number of lines in the file.
 *
 * PUBLIC: int file_lline __P((SCR *, recno_t *));
 */
int
file_lline(sp, lnop)
	SCR *sp;
	recno_t *lnop;
{
	DBT data, key;
	EXF *ep;
	recno_t lno;

	/* Check the cache. */
	ep = sp->ep;
	if (ep->c_nlines != OOBLNO) {
		*lnop = (F_ISSET(sp, S_INPUT) &&
		    ((TEXT *)sp->tiq.cqh_last)->lno > ep->c_nlines ?
		    ((TEXT *)sp->tiq.cqh_last)->lno : ep->c_nlines);
		return (0);
	}

	key.data = &lno;
	key.size = sizeof(lno);

	switch (ep->db->seq(ep->db, &key, &data, R_LAST)) {
        case -1:
		msgq(sp, M_SYSERR, "007|%s/%d: unable to get last line",
		    tail(__FILE__), __LINE__);
		*lnop = 0;
		return (1);
        case 1:
		*lnop = 0;
		return (0);
	default:
		break;
	}

	/* Fill the cache. */
	memmove(&lno, key.data, sizeof(lno));
	ep->c_nlines = ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;

	/* Return the value. */
	*lnop = (F_ISSET(sp, S_INPUT) &&
	    ((TEXT *)sp->tiq.cqh_last)->lno > lno ?
	    ((TEXT *)sp->tiq.cqh_last)->lno : lno);
	return (0);
}

/*
 * file_lerr --
 *	Report a line error.
 *
 * PUBLIC: void file_lerr __P((SCR *, char *, recno_t, recno_t));
 */
void
file_lerr(sp, fname, fline, lno)
	SCR *sp;
	char *fname;
	recno_t fline, lno;
{
	msgq(sp, M_ERR, "008|Error: %s/%d: unable to retrieve line %u",
	    tail(fname), fline, lno);
}

/*
 * scr_update --
 *	Update all of the screens that are backed by the file that
 *	just changed.
 */
static __inline int
scr_update(sp, lno, op, current)
	SCR *sp;
	recno_t lno;
	lnop_t op;
	int current;
{
	EXF *ep;
	SCR *tsp;

	if (F_ISSET(sp, S_EX))
		return (0);

	ep = sp->ep;
	if (ep->refcnt != 1)
		for (tsp = sp->gp->dq.cqh_first;
		    tsp != (void *)&sp->gp->dq; tsp = tsp->q.cqe_next)
			if (sp != tsp && tsp->ep == ep)
				(void)vs_change(tsp, lno, op);
	return (current && vs_change(sp, lno, op));
}
