/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 5.13 1993/03/26 13:37:51 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:37:51 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"

/*
 * XXX
 *
 * Right now, it's expensive to find the marks since we traverse the array
 * linearly.  Should have a doubly linked list of mark entries so we can
 * traverse it quickly on updates.
 */

/*
 * mark_reset --
 *	Reset the marks for file changes.
 */
void
mark_reset(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK m;

	memset(ep->marks, 0, sizeof(ep->marks));
	m.lno = 1;
	m.cno = 0;
	SETABSMARK(sp, ep, &m);
}

/*
 * mark_set --
 *	Set the location referenced by a mark.
 */
int
mark_set(sp, ep, key, mp)
	SCR *sp;
	EXF *ep;
	int key;
	MARK *mp;
{
	if (key > UCHAR_MAX) {
		msgq(sp, M_ERROR, "Invalid mark name.");
		return (1);
	}
	ep->marks[key] = *mp;
	return (0);
}

/*
 * mark_get --
 *	Get the location referenced by a mark.
 */
MARK *
mark_get(sp, ep, key)
	SCR *sp;
	EXF *ep;
	int key;
{
	MARK *mp;

	if (key > UCHAR_MAX) {
		msgq(sp, M_ERROR, "Invalid mark name.");
		return (NULL);
	}
	mp = &ep->marks[key];
	if (mp->lno == OOBLNO) {
		msgq(sp, M_ERROR, "Mark '%c not set.", key);
                return (NULL);
	}
	return (mp);
}

/*
 * Historic vi got mark updates wrong.  Marks were fixed, and subsequent
 * modifications of the line wouldn't update the position of the mark.  This
 * is arguably correct in some cases, e.g. when the user wants to keep track
 * of the start of a line.  However, given that single quote marks mark lines,
 * not specific locations, and that it would be fairly difficult to reproduce
 * the exact vi semantics, these routines do it "correctly".
 *
 * XXX
 * In the historic vi, marks would return if the operation was undone.  This
 * code doesn't handle that problem.  It should be done as part of TXN undo,
 * logged from here.
 */
/*
 * mark_delete --
 *	Update the marks based on a deletion.
 */
void
mark_delete(sp, ep, fm, tm, lmode)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
	int lmode;
{
	register MARK *mp;
	register int cno, cnt, lno;
	
	cno = tm->cno - fm->cno;
	if (tm->lno == fm->lno) {
		lno = fm->lno;
		for (cnt = 0, mp = ep->marks;
		    cnt < sizeof(ep->marks) / sizeof(ep->marks[0]);
		    ++cnt, ++mp) {
			if (mp->lno != lno || mp->cno < fm->cno)
				continue;
			if (lmode || mp->cno < tm->cno)
				mp->lno = OOBLNO;
			else
				mp->cno -= cno;
		}
	} else {
		lno = tm->lno - fm->lno + 1;
		for (cnt = 0, mp = ep->marks;
		    cnt < sizeof(ep->marks) / sizeof(ep->marks[0]);
		    ++cnt, ++mp) {
			if (mp->lno < fm->lno)
				continue;
			if (mp->lno == fm->lno)
				if (lmode || mp->cno >= fm->cno)
					mp->lno = OOBLNO;
				else
					mp->cno -= cno;
			else
				mp->lno -= lno;
		}
	}
}

/*
 * mark_insert --
 *	Update the marks based on an insertion.
 */
void
mark_insert(sp, ep, fm, tm)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
{
	register MARK *mp;
	register int cno, cnt, lno;
	
	lno = tm->lno - fm->lno;
	cno = tm->cno - fm->cno;
	for (cnt = 0, mp = ep->marks;
	    cnt < sizeof(ep->marks) / sizeof(ep->marks[0]); ++cnt, ++mp) {
		if (mp->lno < fm->lno)
			continue;
		if (mp->lno > tm->lno) {
			tm->lno += lno;
			continue;
		}
		if (mp->cno < fm->cno)
			continue;

		mp->lno += lno;
		mp->cno += cno;
	}
}
