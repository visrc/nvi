/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_left.c,v 5.11 1993/02/28 14:01:49 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:01:49 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"

/*
 * v_left -- [count]^H, [count]h
 *	Move left by columns.
 */
int
v_left(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	if (fm->cno == 0) {
		ep->msg(ep, M_BELL, "Already in the first column.");
		return (1);
	}

	rp->lno = fm->lno;
	if (fm->cno > cnt)
		rp->cno = fm->cno - cnt;
	else
		rp->cno = 0;
	return (0);
}

/*
 * v_first -- ^, _
 *	Move to the first non-blank column on this line.
 */
int
v_first(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = fm->lno;
	return (0);
}

/*
 * v_ncol -- [count]|
 *	Move to column count, or the first non-blank column on this line.
 */
int
v_ncol(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = fm->lno;
	if (vp->flags & VC_C1SET)
		rp->cno = vp->count;
	else if (nonblank(ep, fm->lno, &rp->cno))
		rp->cno = 0;
	return (0);
}

/*
 * v_zero -- 0
 *	Move to the first column on this line.
 */
int
v_zero(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = fm->lno;
	rp->cno = 0;
	return (0);
}
