/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 5.9 1993/02/16 20:08:36 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:08:36 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"

/*
 * v_mark -- m[a-z]
 *	Define a mark.
 */
int
v_mark(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (mark_set(ep, vp->character, fm));
}

/*
 * v_markbt -- '['`a-z]
 *	Move to a mark.
 */
int
v_markbt(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;

	if ((mp = mark_get(ep, vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}

/*
 * v_marksq -- '['`a-z]
 *	Move to the first nonblank character of a line containing a mark.
 */
int
v_marksq(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;

	if ((mp = mark_get(ep, vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}
