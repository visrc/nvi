/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: db.c,v 8.5 1993/08/06 22:56:17 bostic Exp $ (Berkeley) $Date: 1993/08/06 22:56:17 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <string.h>

#include "vi.h"
#include "recover.h"

static inline int scr_update __P((SCR *, EXF *, recno_t, enum operation, int));

/*
 * file_gline --
 *	Look in the text buffers for a line; if it's not there
 *	call file_rline to retrieve it from the database.
 */
char *
file_gline(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
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
	return (file_rline(sp, ep, lno, lenp));
}

/*
 * file_rline --
 *	Look in the cache for a line; if it's not there retrieve
 *	it from the file.
 */
char *
file_rline(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;

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
	switch (ep->db->get(ep->db, &key, &data, 0)) {
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

#if DEBUG && 0
	TRACE(sp, "delete line %lu\n", lno);
#endif
	/* Log change. */
	log_line(sp, ep, lno, LOG_LINE_DELETE);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	if (ep->db->del(ep->db, &key, 0) == 1) {
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
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Update screen. */
	return (scr_update(sp, ep, lno, LINE_DELETE, 1));
}

/*
 * file_aline --
 *	Append a line into the file.
 */
int
file_aline(sp, ep, update, lno, p, len)
	SCR *sp;
	EXF *ep;
	int update;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	recno_t lline;

#if DEBUG && 0
	TRACE(sp, "append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/*
	 * Very nasty special case.  The historic vi code displays a single
	 * space (or a '$' if the list option is set) for the first line in
	 * an "empty" file.  If we "insert" a line, that line gets scrolled
	 * down, not repainted, so it's incorrect when we refresh the the
	 * screen.  This is really hard to find and fix in the vi code -- the
	 * text input functions detect it explicitly and don't insert a new
	 * line.  The hack here is to repaint the screen if we're appending
	 * to an empty file.
	 */
	if (lno == 0) {
		if (file_lline(sp, ep, &lline))
			return (1);
		if (lline == 0)
			F_SET(sp, S_REDRAW);
	}

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if (ep->db->put(ep->db, &key, &data, R_IAFTER) == -1) {
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
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno + 1, LOG_LINE_APPEND);

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
	return (scr_update(sp, ep, lno, LINE_APPEND, update));
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
	recno_t lline;

#if DEBUG && 0
	TRACE(sp,
	    "insert before %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif

	/* Very nasty special case.  See comment in file_aline(). */
	if (lno == 1) {
		if (file_lline(sp, ep, &lline))
			return (1);
		if (lline == 0)
			F_SET(sp, S_REDRAW);
	}

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if (ep->db->put(ep->db, &key, &data, R_IBEFORE) == -1) {
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
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno, LOG_LINE_INSERT);

	/* Update screen. */
	return (scr_update(sp, ep, lno, LINE_INSERT, 1));
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

#if DEBUG && 0
	TRACE(sp,
	    "replace line %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Log before change. */
	log_line(sp, ep, lno, LOG_LINE_RESET_B);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if (ep->db->put(ep->db, &key, &data, 0) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to store line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, before logging or screen update. */
	if (lno == ep->c_lno)
		ep->c_lno = OOBLNO;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, ep, lno, LOG_LINE_RESET_F);
	
	/* Update screen. */
	return (scr_update(sp, ep, lno, LINE_RESET, 1));
}

/*
 * file_lline --
 *	Return the number of lines in the file.
 */
int
file_lline(sp, ep, lnop)
	SCR *sp;
	EXF *ep;
	recno_t *lnop;
{
	DBT data, key;
	recno_t lno;

	/* Check the cache. */
	if (ep->c_nlines != OOBLNO) {
		*lnop = (F_ISSET(sp, S_INPUT) &&
		    ((TEXT *)sp->txthdr.prev)->lno > ep->c_nlines ?
		    ((TEXT *)sp->txthdr.prev)->lno : ep->c_nlines);
		return (0);
	}

	key.data = &lno;
	key.size = sizeof(lno);

	switch (ep->db->seq(ep->db, &key, &data, R_LAST)) {
        case -1:
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to get last line: %s.",
		    tail(__FILE__), __LINE__, strerror(errno));
		*lnop = 0;
		return (1);
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
 
	*lnop = (F_ISSET(sp, S_INPUT) &&
	    ((TEXT *)sp->txthdr.prev)->lno > lno ?
	    ((TEXT *)sp->txthdr.prev)->lno : lno);
	return (0);
}

/*
 * scr_update --
 *	Walk the screens and update all of them that are backed by the
 *	file that just changed.  Do the current screen last so that the
 *	cursor ends up in the right place.
 *
 * XXX
 * Don't refresh the current screen, assume that it will be refreshed
 * by the caller when the caller is done.  This is wrong, we shouldn't
 * have to refresh all of the screens this much.  It should be possible
 * to just have an scr_refresh() routine that refreshed all related
 * screens, and replace all occurrences of s_refresh with it?
 *
 * XXX
 * This routine probably depends on the "current" flag not being unset
 * during editing -- it seems to me that there may be ways to make the
 * cursor end up on the wrong screen after an update.  I think the
 * underlying assumption is that the "current" screen will be refreshed
 * after the updates to the other screens are done:
 *
 *	the user edits the text using v_ntext(), and the
 *		current screen is updated as keys are entered.
 *	the current screen is resolved, related screens
 *		updated here
 *	the current screen is refreshed and the cursor returns
 *		to the right position.
 */
static inline int
scr_update(sp, ep, lno, op, current)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum operation op;
	int current;
{
	SCR *tsp;

	if (ep->refcnt != 1) {
		for (tsp = sp->parent; tsp != NULL; tsp = tsp->parent)
			if (tsp->ep == ep && tsp->s_change != NULL) {
				(void)sp->s_change(tsp, ep, lno, op);
				(void)sp->s_refresh(tsp, ep);
			}
		for (tsp = sp->child; tsp != NULL; tsp = tsp->child)
			if (tsp->ep == ep && tsp->s_change != NULL) {
				(void)sp->s_change(tsp, ep, lno, op);
				(void)sp->s_refresh(tsp, ep);
			}
	}
	if (current && sp->s_change != NULL)
		return (sp->s_change(sp, ep, lno, op));
	return (0);
}
