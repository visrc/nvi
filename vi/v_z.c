/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_z.c,v 5.24 1993/02/24 13:01:45 bostic Exp $ (Berkeley) $Date: 1993/02/24 13:01:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "screen.h"
#include "term.h"
#include "vcmd.h"

/*
 * v_z -- [count]z[count][.-<CR>]
 *	Move the screen.
 */
int
v_z(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last, lno;

	/*
	 * The first count is the line to use.  If the value doesn't
	 * exist, use the last line.
	 */
	if (vp->flags & VC_C1SET) {
		lno = vp->count;
		last = file_lline(ep);
		if (lno > last)
			lno = last;
	} else
		lno = fm->lno;

	/* The second count is the window size. */
	if (vp->flags & VC_C2SET && set_window_size(ep, vp->count2))
		return (1);

	switch(vp->character) {
	case '.':
		if (scr_sm_fill(ep, lno, P_MIDDLE))
			return (1);
		break;
	case '-':
		if (scr_sm_fill(ep, lno, P_BOTTOM))
			return (1);
		break;
	default:
		if (special[vp->character] == K_CR) {
			if (scr_sm_fill(ep, lno, P_TOP))
				return (1);
			break;
		}
		msg(ep, M_ERROR, "usage: %s.", vp->kp->usage);
		return (1);
	}

	/* If the map changes, have to redraw the entire screen. */
	SF_SET(ep, S_REDRAW);

	rp->lno = lno;
	rp->cno = fm->cno;
	return (0);
}
