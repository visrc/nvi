/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 5.11 1993/03/26 13:40:32 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:40:32 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_mark -- m[a-z]
 *	Define a mark.
 */
int
v_mark(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (mark_set(sp, ep, vp->character, fm));
}

/*
 * v_markbt -- '['`a-z]
 *	Move to a mark.
 */
int
v_markbt(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;

	if ((mp = mark_get(sp, ep, vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}

/*
 * v_marksq -- '['`a-z]
 *	Move to the first nonblank character of a line containing a mark.
 */
int
v_marksq(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;

	if ((mp = mark_get(sp, ep, vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}
