/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 8.3 1993/12/28 17:05:15 bostic Exp $ (Berkeley) $Date: 1993/12/28 17:05:15 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Undo -- U
 *	Undo changes to this line.
 */
int
v_Undo(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * Historically, U reset the cursor to the first column in the line
	 * (not the first non-blank).  This seems a bit non-intuitive, but,
	 * considering that we may have undone multiple changes, anything
	 * else (including the cursor position stored in the logging records)
	 * is going to appear random.
	 */
	rp->lno = fm->lno;
	rp->cno = 0;
	return (log_setline(sp, ep));
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
	/*
	 * !!!
	 * In historic vi, 'u' toggled between "undo" and "redo", i.e. 'u'
	 * undid the last undo.  To make this work when the user can undo
	 * multiple operations, we leave the old semantic unchanged, but
	 * make '.' do another operation (undo or redo) in the same direction
	 * as the last 'u' command, unless there was a change since the last
	 * undo, in which case we always undo the last operation.  Since
	 * 'u' didn't set '.' in historic vi, the breakage should be limited.
	 */
	if (!F_ISSET(ep, F_UNDO))
		ep->lundo = BACKWARD;
	else if (!F_ISSET(vp, VC_ISDOT))
		ep->lundo = ep->lundo == BACKWARD ? FORWARD : BACKWARD;

	F_SET(ep, F_UNDO);
	switch (ep->lundo) {
	case BACKWARD:
		return (log_backward(sp, ep, rp));
	case FORWARD:
		return (log_forward(sp, ep, rp));
	default:
		abort();
	}
	/* NOTREACHED */
}
