/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_z.c,v 5.22 1993/02/16 20:09:14 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:09:14 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "term.h"

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
	recno_t lno;

	if (vp->flags & VC_C1SET) {
		fm->lno = vp->count;
		if (file_gline(ep, fm->lno, NULL) == NULL)
			fm->lno = file_lline(ep);
	}
	lno = fm->lno;

	/*
	 * XXX
	 * The second count is the window size.  This needs to be
	 * implemented when the general resizing routines are done.
	 */

	switch(vp->character) {
	case '.':
		if (lno <= HALFSCREEN(ep))
			break;
		lno -= HALFSCREEN(ep);
		if (file_gline(ep, lno, NULL) == NULL)
			break;
		ep->top = lno;
		break;
	case '-':
		if (lno <= SCREENSIZE(ep))
			break;
		lno -= SCREENSIZE(ep) - 1;
		if (file_gline(ep, lno, NULL) == NULL)
			break;
		ep->top = lno;
		break;
	default:
		if (special[vp->character] == K_CR) {
			ep->top = lno;
			break;
		}
		msg(ep, M_ERROR, "usage: %s.", vp->kp->usage);
		return (1);
	}
	*rp = *fm;
	return (0);
}
