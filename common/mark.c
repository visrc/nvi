/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 5.4 1992/05/27 10:30:34 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:30:34 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "mark.h"
#include "exf.h"
#include "extern.h"

/*
 * XXX
 * Right now, it's expensive to find the marks since we traverse the array
 * linearly.  Should have a doubly linked list of mark entries so we can
 * traverse it quickly on updates.
 */
static MARK marks[UCHAR_MAX + 1];

int
mark_set(key, mp)
	int key;
	MARK *mp;
{
	if (key > UCHAR_MAX) {
		bell();
		msg("Invalid mark name.");
		return (1);
	}
	marks[key] = *mp;
	return (0);
}

MARK *
mark_get(key)
	int key;
{
	MARK *mp;

	if (key > UCHAR_MAX) {
		bell();
		msg("Invalid mark name.");
		return (NULL);
	}
	mp = &marks[key];
	if (mp->lno == OOBLNO) {
		bell();
		msg("Mark '%c not set.", key);
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
mark_delete(fm, tm, lmode)
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
				mp->lno == OOBLNO;
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
					mp->lno == OOBLNO;
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
mark_insert(fm, tm)
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
