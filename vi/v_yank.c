/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 5.16 1993/03/26 13:41:00 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:41:00 $";
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
	return (cut(sp, ep, VICB(vp), fm, tm, vp->flags & VC_LMODE));
}
