/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 5.12 1993/05/08 16:09:47 bostic Exp $ (Berkeley) $Date: 1993/05/08 16:09:47 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_mark -- m[a-z]
 *	Set a mark.
 */
int
v_mark(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = fm->lno;
	rp->cno = fm->cno;
	return (mark_set(sp, ep, vp->character, fm));
}

/*
 * v_gomark -- '['`a-z], or `['`a-z]
 *	Move to a mark.
 *
 *	The single quote form moves to the first nonblank character of a line
 *	containing a mark.  The back quote form moves to a mark, setting both
 *	row and column.  We use a single routine for both forms, taking care
 *	of the nonblank behavior using a flag for the command.
 *
 *	Although not commonly known, the "'`" and "'`" forms are historically
 *	valid.  The behavior is determined by the first character, so "`'" is
 *	the same as "``".  Remember this fact -- you'll be amazed at how many
 *	people don't know it and will be delighted that you are able to tell
 *	them.
 */
int
v_gomark(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp, m;

	/* If a single or back quote, go to the last absolute mark. */
	if (vp->character == '\'' || vp->character == '`') {
		*rp = ep->labsmark;
		return (0);
	}
		
	if ((mp = mark_get(sp, ep, vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}
