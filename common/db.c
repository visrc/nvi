/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: db.c,v 5.23 1993/03/26 13:37:45 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:37:45 $";
#endif /* not lint */

#include <sys/param.h>

#include <errno.h>
#include <string.h>

#include "vi.h"

/*
 * file_gline --
 *	Retrieve a line from the file.
 */
u_char *
file_gline(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;
	TEXT *tp;
	recno_t cnt;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but
	 * have to have an oob condition for the look-aside into the input
	 * buffer anyway.
	 */
	if (lno == 0)
		return (NULL);

	/*
	 * Look-aside into the input buffer and see if the line we want is
	 * there.
	 */
	if (ib.stop.lno >= lno && ib.start.lno <= lno) {
		if (ib.stop.lno == lno) {
			if (lenp)
				*lenp = ib.len;
			return (ib.ilb);
		}
		for (cnt = ib.start.lno, tp = ib.head; cnt < lno; ++cnt)
			tp = tp->next;
		if (lenp)
			*lenp = tp->len;
		return (tp->lp);
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
		msgq(sp, M_ERROR,
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
		msgq(sp, M_ERROR,
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
	if (F_ISSET(sp, S_MODE_VI))
		scr_change(sp, ep, lno, LINE_DELETE);
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
	u_char *p;
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
		msgq(sp, M_ERROR,
		    "Error: %s/%d: unable to append to line %u: %s.",
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
	log_line(sp, ep, lno + 1, LINE_APPEND);

	/* Update screen. */
	if (F_ISSET(sp, S_MODE_VI))
		scr_change(sp, ep, lno, LINE_APPEND);
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
	u_char *p;
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
		msgq(sp, M_ERROR,
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
	if (F_ISSET(sp, S_MODE_VI))
		scr_change(sp, ep, lno, LINE_INSERT);
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
	u_char *p;
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
		msgq(sp, M_ERROR,
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
	if (F_ISSET(sp, S_MODE_VI))
		scr_change(sp, ep, lno, LINE_RESET);
	return (0);
}

/*
 * file_ibresolv --
 *	Resolve the ib structure into the file.
 */
int
file_ibresolv(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	DBT data, key;
	TEXT *tp;

	/* Setup. */
	tp = ib.head;
	key.size = sizeof(lno);

	/* Replace the original line. */
	key.data = &lno;
	data.data = tp->lp;
	data.size = tp->len;
	if ((ep->db->put)(ep->db, &key, &data, 0) == -1) {
		msgq(sp, M_ERROR,
		    "Error: %s/%d: unable to store line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Add the new lines into the file. */
	for (; tp = tp->next; ++lno) {
		key.data = &lno;
		data.data = tp->lp;
		data.size = tp->len;
		if ((ep->db->put)(ep->db, &key, &data, R_IAFTER) == -1) {
			msgq(sp, M_ERROR,
			    "Error: %s/%d: unable to store line %u: %s.",
			    tail(__FILE__), __LINE__, lno, strerror(errno));
			return (1);
		}
	}

	/* Flush the cache, update line count. */
	ep->c_lno = OOBLNO;
	ep->c_nlines = OOBLNO;

	/* File now dirty. */
	F_SET(ep, F_MODIFIED);
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
		return (ib.stop.lno > ep->c_nlines ?
		    ib.stop.lno : ep->c_nlines);

	key.data = &lno;
	key.size = sizeof(lno);

	switch((ep->db->seq)(ep->db, &key, &data, R_LAST)) {
        case -1:
		msgq(sp, M_ERROR,
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

	return (ib.stop.lno > lno ? ib.stop.lno : lno);
}
