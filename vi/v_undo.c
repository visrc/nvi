/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.23 1993/05/17 16:47:32 bostic Exp $ (Berkeley) $Date: 1993/05/17 16:47:32 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Undo -- U
 *	Undo changes to this line, or roll forward.
 *
 *	Historic vi moved the cursor to the first non-blank character
 *	of the line when this happened.  We do not, since we know where
 *	the cursor actually was when the changes began.
 */
int
v_Undo(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (O_ISSET(sp, O_NUNDO))
		return (log_forward(sp, ep, rp));
	return (log_setline(sp, ep, rp));
}
	
/*
 * v_undo -- u
 *	Undo the last change.
 */
int
v_undo(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (O_ISSET(sp, O_NUNDO))
		return (log_backward(sp, ep, rp));

	if (!F_ISSET(ep, F_UNDO)) {
		ep->lundo = UFORWARD;
		F_SET(ep, F_UNDO);
	}

	switch (ep->lundo) {
	case UBACKWARD:
		if (log_forward(sp, ep, rp)) {
			F_CLR(ep, F_UNDO);
			return (1);
		}
		ep->lundo = UFORWARD;
		break;
	case UFORWARD:
		if (log_backward(sp, ep, rp)) {
			F_CLR(ep, F_UNDO);
			return (1);
		}
		ep->lundo = UBACKWARD;
		break;
	}
	return (0);
}
