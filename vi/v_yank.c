/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 5.13 1992/12/05 11:11:11 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:11:11 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Yank -- [buffer][count]y
 *	Yank lines of text into a cut buffer.
 */
int
v_Yank(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	*rp = *fm;
	curf->rptlines = tm->lno - fm->lno + 1;
	curf->rptlabel = "yanked";
	return (cut(curf, VICB(vp), fm, tm, 1));
}

/*
 * v_yank -- [buffer][count]y[count][motion]
 *	Yank text into a cut buffer.
 */
int
v_yank(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	*rp = *fm;
	curf->rptlines = tm->lno - fm->lno + 1;
	curf->rptlabel = "yanked";
	return (cut(curf, VICB(vp), fm, tm, vp->flags & VC_LMODE));
}
