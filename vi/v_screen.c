/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_screen.c,v 5.4 1993/05/10 11:37:35 bostic Exp $ (Berkeley) $Date: 1993/05/10 11:37:35 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_window --
 *	Switch windows.
 */
int
v_window(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	SCR *p;

	*rp = *fm;

	/*
	 * Try for the next lower window, or, go back to the first
	 * window on the stack.
	 */
	if (sp->child != NULL)
		sp->snext = sp->child;
	else if (sp->parent == NULL) {
		msgq(sp, M_ERR, "No other window to switch to");
		return (1);
	} else {
		for (p = sp; p->parent != NULL; p = p->parent);
		sp->snext = p;
	}
	
	F_SET(sp, S_SSWITCH);
	return (0);
}
