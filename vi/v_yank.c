/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 8.3 1993/08/06 11:47:32 bostic Exp $ (Berkeley) $Date: 1993/08/06 11:47:32 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_yank --	[buffer][count]Y
 *		[buffer][count]y[count][motion]
 *	Yank text (or lines of text) into a cut buffer.
 */
int
v_yank(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	rp->lno = sp->lno;
	rp->cno = sp->cno;
	if (cut(sp, ep, VICB(vp), fm, tm, F_ISSET(vp, VC_LMODE)))
		return (1);
	sp->rptlines[L_YANKED] += tm->lno - fm->lno + 1;
	return (0);
}
