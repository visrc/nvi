/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.18 1993/03/25 15:01:41 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:01:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "log.h"
#include "options.h"
#include "screen.h"
#include "vcmd.h"

/*
 * v_Undo -- U
 *	Undo changes to this line, or roll forward.
 */
int
v_Undo(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (ISSET(O_NUNDO) ?
	    log_forward(sp, ep, rp) : log_backward(sp, ep, rp, ep->lno));
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
	if (ISSET(O_NUNDO))
		return (log_backward(sp, ep, rp, OOBLNO));

	if (!F_ISSET(ep, F_UNDO)) {
		ep->lundo = UFORWARD;
		F_SET(ep, F_UNDO);
	}

	switch(ep->lundo) {
	case UBACKWARD:
		if (log_forward(sp, ep, rp)) {
			F_CLR(ep, F_UNDO);
			return (1);
		}
		ep->lundo = UFORWARD;
		break;
	case UFORWARD:
		if (log_backward(sp, ep, rp, OOBLNO)) {
			F_CLR(ep, F_UNDO);
			return (1);
		}
		ep->lundo = UBACKWARD;
		break;
	}
	return (0);
}
