/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_left.c,v 5.15 1993/05/27 22:57:38 bostic Exp $ (Berkeley) $Date: 1993/05/27 22:57:38 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_left -- [count]^H, [count]h
 *	Move left by columns.
 */
int
v_left(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	if (fm->cno == 0) {
		msgq(sp, M_BERR, "Already in the first column.");
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
v_first(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = fm->lno;
	return (0);
}

/*
 * v_ncol -- [count]|
 *	Move to column count or the first column on this line.  If the
 *	requested column is past EOL, move to EOL.  The nasty part is
 *	that we have to know character column widths to make this work.
 */
int
v_ncol(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (F_ISSET(vp, VC_C1SET) && vp->count > 1)
		rp->cno =
		    sp->s_chposition(sp, ep, fm->lno, (size_t)--vp->count);
	else
		rp->cno = 0;
	rp->lno = fm->lno;
	return (0);
}

/*
 * v_zero -- 0
 *	Move to the first column on this line.
 */
int
v_zero(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = fm->lno;
	rp->cno = 0;
	return (0);
}
