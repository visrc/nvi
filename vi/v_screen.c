/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_screen.c,v 8.8 1993/11/28 18:31:30 bostic Exp $ (Berkeley) $Date: 1993/11/28 18:31:30 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_screen --
 *	Switch screens.
 */
int
v_screen(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * Try for the next lower screen, or, go back to the first
	 * screen on the stack.
	 */
	if (sp->q.cqe_next != (void *)&sp->gp->dq)
		sp->nextdisp = sp->q.cqe_next;
	else if (sp->gp->dq.cqh_first == sp) {
		msgq(sp, M_ERR, "No other screen to switch to.");
		return (1);
	} else
		sp->nextdisp = sp->gp->dq.cqh_first;

	/*
	 * Display the old screen's status line so the user can
	 * find the screen they want.
	 */
	(void)status(sp, ep, fm->lno, 0);

	/* Save the old screen's cursor information. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	F_SET(sp, S_SSWITCH);
	return (0);
}
