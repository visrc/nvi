/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_z.c,v 8.1 1993/06/09 22:28:53 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:28:53 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_z -- [count]z[count][.-<CR>]
 *	Move the screen.
 */
int
v_z(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last, lno;

	/*
	 * The first count is the line to use.  If the value doesn't
	 * exist, use the last line.
	 */
	if (F_ISSET(vp, VC_C1SET)) {
		lno = vp->count;
		if (file_lline(sp, ep, &last))
			return (1);
		if (lno > last)
			lno = last;
	} else
		lno = fm->lno;

	/* The second count is the window size. */
	if (F_ISSET(vp, VC_C2SET) && set_window_size(sp, vp->count2))
		return (1);

	switch(vp->character) {
	case '.':
		if (sp->s_fill(sp, ep, lno, P_MIDDLE))
			return (1);
		break;
	case '-':
		if (sp->s_fill(sp, ep, lno, P_BOTTOM))
			return (1);
		break;
	default:
		if (sp->special[vp->character] == K_CR) {
			if (sp->s_fill(sp, ep, lno, P_TOP))
				return (1);
			break;
		}
		msgq(sp, M_ERR, "usage: %s.", vp->kp->usage);
		return (1);
	}

	/* If the map changes, have to redraw the entire screen. */
	F_SET(sp, S_REDRAW);

	rp->lno = lno;
	rp->cno = fm->cno;
	return (0);
}
