/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 8.1 1993/06/09 22:28:48 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:28:48 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Yank -- [buffer][count]y
 *	Yank lines of text into a cut buffer.
 */
int
v_Yank(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = sp->lno;
	rp->cno = sp->cno;
	sp->rptlines[L_YANKED] += tm->lno - fm->lno + 1;
	return (cut(sp, ep, VICB(vp), fm, tm, 1));
}

/*
 * v_yank -- [buffer][count]y[count][motion]
 *	Yank text into a cut buffer.
 */
int
v_yank(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	rp->lno = sp->lno;
	rp->cno = sp->cno;
	sp->rptlines[L_YANKED] += tm->lno - fm->lno + 1;
	return (cut(sp, ep, VICB(vp), fm, tm, F_ISSET(vp, VC_LMODE)));
}
