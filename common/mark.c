/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 5.11 1993/02/28 13:59:12 bostic Exp $ (Berkeley) $Date: 1993/02/28 13:59:12 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"

/*
 * XXX
 * Right now, it's expensive to find the marks since we traverse the array
 * linearly.  Should have a doubly linked list of mark entries so we can
 * traverse it quickly on updates.
 */
static MARK marks[UCHAR_MAX + 1];

/*
 * mark_reset --
 *	Reset the marks for file changes.
 */
void
mark_reset(ep)
	EXF *ep;
{
	MARK m;

	memset(marks, 0, sizeof(marks));
	m.lno = 1;
	m.cno = 0;
	SETABSMARK(ep, &m);
}

/*
 * mark_set --
 *	Set the location referenced by a mark.
 */
int
mark_set(ep, key, mp)
	EXF *ep;
	int key;
	MARK *mp;
{
	if (key > UCHAR_MAX) {
		ep->msg(ep, M_ERROR, "Invalid mark name.");
		return (1);
	}
	marks[key] = *mp;
	return (0);
}

/*
 * mark_get --
 *	Get the location referenced by a mark.
 */
MARK *
mark_get(ep, key)
	EXF *ep;
	int key;
{
	MARK *mp;

	if (key > UCHAR_MAX) {
		ep->msg(ep, M_ERROR, "Invalid mark name.");
		return (NULL);
	}
	mp = &marks[key];
	if (mp->lno == OOBLNO) {
		ep->msg(ep, M_ERROR, "Mark '%c not set.", key);
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
mark_delete(ep, fm, tm, lmode)
	EXF *ep;
	MARK *fm, *tm;
	int lmode;
{
	register MARK *mp;
	register int cno, cnt, lno;
	
	cno = tm->cno - fm->cno;
	if (tm->lno == fm->lno) {
		lno = fm->lno;
		for (cnt = 0, mp = marks;
		    cnt < sizeof(marks) / sizeof(marks[0]); ++cnt, ++mp) {
			if (mp->lno != lno || mp->cno < fm->cno)
				continue;
			if (lmode || mp->cno < tm->cno)
				mp->lno = OOBLNO;
			else
				mp->cno -= cno;
		}
	} else {
		lno = tm->lno - fm->lno + 1;
		for (cnt = 0, mp = marks;
		    cnt < sizeof(marks) / sizeof(marks[0]); ++cnt, ++mp) {
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
mark_insert(ep, fm, tm)
	EXF *ep;
	MARK *fm, *tm;
{
	register MARK *mp;
	register int cno, cnt, lno;
	
	lno = tm->lno - fm->lno;
	cno = tm->cno - fm->cno;
	for (cnt = 0, mp = marks;
	    cnt < sizeof(marks) / sizeof(marks[0]); ++cnt, ++mp) {
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
