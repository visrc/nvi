/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_z.c,v 5.20 1992/10/29 14:45:04 bostic Exp $ (Berkeley) $Date: 1992/10/29 14:45:04 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

/*
 * v_z -- [count]z[count][.-<CR>]
 *	Move the screen.
 */
int
v_z(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	if (vp->flags & VC_C1SET) {
		fm->lno = vp->count;
		if (file_gline(curf, fm->lno, NULL) == NULL)
			fm->lno = file_lline(curf);
	}
	lno = fm->lno;

	/*
	 * XXX
	 * The second count is the window size.  This needs to be
	 * implemented when the general resizing routines are done.
	 */

	switch(vp->character) {
	case '.':
		if (lno <= HALFSCREEN(curf))
			break;
		lno -= HALFSCREEN(curf);
		if (file_gline(curf, lno, NULL) == NULL)
			break;
		curf->top = lno;
		break;
	case '-':
		if (lno <= SCREENSIZE(curf))
			break;
		lno -= SCREENSIZE(curf) - 1;
		if (file_gline(curf, lno, NULL) == NULL)
			break;
		curf->top = lno;
		break;
	default:
		if (special[vp->character] == K_CR) {
			curf->top = lno;
			break;
		}
		msg("usage: %s.", vp->kp->usage);
		return (1);
	}
	*rp = *fm;
	return (0);
}
