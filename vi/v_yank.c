/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 5.17 1993/04/12 14:58:22 bostic Exp $ (Berkeley) $Date: 1993/04/12 14:58:22 $";
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
	*rp = *fm;
	sp->rptlines = tm->lno - fm->lno + 1;
	sp->rptlabel = "yanked";
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
	*rp = *fm;
	sp->rptlines = tm->lno - fm->lno + 1;
	sp->rptlabel = "yanked";
	return (cut(sp, ep, VICB(vp), fm, tm, F_ISSET(vp, VC_LMODE)));
}
