/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 5.5 1992/05/27 10:36:30 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:36:30 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_mark -- m[a-z]
 *	Define a mark.
 */
int
v_mark(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (mark_set(vp->character, fm));
}

/*
 * v_markbt -- '['`a-z]
 *	Move to a mark.
 */
int
v_markbt(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;

	if ((mp = mark_get(vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}

/*
 * v_marksq -- '['`a-z]
 *	Move to the first nonblank character of a line containing a mark.
 */
int
v_marksq(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;

	if ((mp = mark_get(vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (v_nonblank(rp));
}
