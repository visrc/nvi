/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_screen.c,v 8.4 1993/11/16 10:37:27 bostic Exp $ (Berkeley) $Date: 1993/11/16 10:37:27 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_screen --
 *	Switch screens.
 */
int
v_screens(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	SCR *p;

	/*
	 * Try for the next lower screen, or, go back to the first
	 * screen on the stack.
	 */
	if (sp->child != NULL)
		sp->snext = sp->child;
	else if (sp->parent == NULL) {
		msgq(sp, M_ERR, "No other screen to switch to");
		return (1);
	} else {
		for (p = sp; p->parent != NULL; p = p->parent);
		sp->snext = p;
	}

	/*
	 * Display the old screen's status line so the user can
	 * find the screen they want.
	 */
	(void)status(sp, ep, fm->lno, 0);
	(void)sp->s_refresh(sp, ep);

	/* Save the old screen's cursor information. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	F_SET(sp, S_SSWITCH);
	return (0);
}
