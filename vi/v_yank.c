/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 8.5 1993/08/19 14:58:29 bostic Exp $ (Berkeley) $Date: 1993/08/19 14:58:29 $";
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
	rp->lno = fm->lno;
	rp->cno = fm->cno;

	tm->lno = fm->lno + (F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0);

	if (F_ISSET(vp, VC_LMODE)) {
		if (file_gline(sp, ep, tm->lno, NULL) == NULL) {
			v_eof(sp, ep, rp);
			return (1);
		}
		if (cut(sp, ep, VICB(vp), fm, tm, 1))
			return (1);
	} else if (cut(sp, ep, VICB(vp), fm, tm, 0))
		return (1);

	sp->rptlines[L_YANKED] += tm->lno - fm->lno + 1;
	return (0);
}
