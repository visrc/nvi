/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: db.c,v 5.29 1993/05/06 01:08:22 bostic Exp $ (Berkeley) $Date: 1993/05/06 01:08:22 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <string.h>

#include "vi.h"

#define	UPDATE_SCREENS(op) {						\
	if (ep->refcnt == 1) {						\
		if (sp->change != NULL)					\
			sp->change(sp, ep, lno, op);			\
	} else {							\
		SCR *__tsp;						\
		for (__tsp = sp->gp->scrhdr.next;			\
		    __tsp != (SCR *)&sp->gp->scrhdr;			\
		    __tsp = __tsp->next)				\
			if (__tsp->ep == ep && __tsp->change != NULL) {	\
				sp->change(__tsp, ep, lno, op);		\
				sp->srefresh(__tsp, ep);		\
			}						\
	}								\
}

/*
 * file_gline --
 *	Retrieve a line from the file.
 */
char *
file_gline(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;
	TEXT *tp;

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
	if (F_ISSET(&sp->txthdr, HDR_INUSE) &&
	    ((TEXT *)sp->txthdr.next)->lno <= lno &&
	    ((TEXT *)sp->txthdr.prev)->lno >= lno) {
		for (tp = sp->txthdr.next; tp->lno != lno; tp = tp->next);
		if (lenp)
			*lenp = tp->len;
		return (tp->lb);
	}
	if (F_ISSET(&sp->bhdr, HDR_INUSE) &&
	    ((TEXT *)sp->bhdr.next)->lno <= lno &&
	    ((TEXT *)sp->bhdr.prev)->lno >= lno) {
		for (tp = sp->bhdr.next; tp->lno != lno; tp = tp->next);
		if (lenp)
			*lenp = tp->len;
		return (tp->lb);
	}

	/* Check the cache. */
	if (lno == ep->c_lno) {
		if (lenp)
			*lenp = ep->c_len;
		return (ep->c_lp);
	}
	ep->c_lno = OOBLNO;

	/* Get the line from the underlying database. */
	key.data = &lno;
	key.size = sizeof(lno);
	switch((ep->db->get)(ep->db, &key, &data, 0)) {
        case -1:
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to get line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		/* FALLTHROUGH */
        case 1:
		return (NULL);
		/* NOTREACHED */
	}
	if (lenp)
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
 */
int
file_dline(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	DBT key;

#if DEBUG && 1
	TRACE(sp, "delete line %lu\n", lno);
#endif

	/* Log change. */
	log_line(sp, ep, lno, LINE_DELETE);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	if ((ep->db->del)(ep->db, &key, 0) == 1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to delete line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno <= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		--ep->c_nlines;

	/* File now dirty. */
	F_SET(ep, F_MODIFIED);

	/* Update screen. */
	UPDATE_SCREENS(LINE_DELETE);
	return (0);
}

/*
 * file_aline --
 *	Append a line into the file.
 */
int
file_aline(sp, ep, lno, p, len)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

#if DEBUG && 1
	TRACE(sp, "append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if ((ep->db->put)(ep->db, &key, &data, R_IAFTER) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to append to line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno < ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno + 1, LINE_APPEND);

	/*
	 * Update screen.
	 *
	 * XXX
	 * Nasty hack, that I'm not real happy with.  If multiple lines are
	 * input by the user, they aren't committed until an <ESC> is entered.
	 * The problem is that the screen was updated/scrolled as each line
	 * was entered.  So, when this routine is called to copy the new lines
	 * from the cut buffer into the file, it has to know not to update the
	 * screen again.
	 */ 
	if (!F_ISSET(sp, S_INPUT))
		UPDATE_SCREENS(LINE_APPEND);
	return (0);
}

/*
 * file_iline --
 *	Insert a line into the file.
 */
int
file_iline(sp, ep, lno, p, len)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

#if DEBUG && 1
	TRACE(sp,
	    "insert before %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if ((ep->db->put)(ep->db, &key, &data, R_IBEFORE) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to insert at line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno >= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno, LINE_INSERT);

	/* Update screen. */
	UPDATE_SCREENS(LINE_INSERT);
	return (0);
}

/*
 * file_sline --
 *	Store a line in the file.
 */
int
file_sline(sp, ep, lno, p, len)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

#if DEBUG && 1
	TRACE(sp,
	    "replace line %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Log change. */
	log_line(sp, ep, lno, LINE_RESET);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if ((ep->db->put)(ep->db, &key, &data, 0) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to store line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, before logging or screen update. */
	if (lno == ep->c_lno)
		ep->c_lno = OOBLNO;

	/* File now dirty. */
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno, LINE_RESET);
	
	/* Update screen. */
	UPDATE_SCREENS(LINE_RESET);
	return (0);
}

/*
 * file_lline --
 *	Return the number of lines in the file.
 */
recno_t
file_lline(sp, ep)
	SCR *sp;
	EXF *ep;
{
	DBT data, key;
	recno_t lno;

	/* Check the cache. */
	if (ep->c_nlines != OOBLNO)
		return (F_ISSET(sp, S_INPUT) &&
		    ((TEXT *)sp->txthdr.prev)->lno > ep->c_nlines ?
		    ((TEXT *)sp->txthdr.prev)->lno : ep->c_nlines);

	key.data = &lno;
	key.size = sizeof(lno);

	switch((ep->db->seq)(ep->db, &key, &data, R_LAST)) {
        case -1:
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to get last line: %s.",
		    tail(__FILE__), __LINE__, strerror(errno));
		/* FALLTHROUGH */
        case 1:
		lno = 0;
		break;
	default:
		memmove(&lno, key.data, sizeof(lno));
		break;
	}

	/* Fill the cache. */
	ep->c_nlines = ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;
 
	return (F_ISSET(sp, S_INPUT) &&
	    ((TEXT *)sp->txthdr.prev)->lno > lno ?
	    ((TEXT *)sp->txthdr.prev)->lno : lno);
}
