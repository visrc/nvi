/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_screen.c,v 5.1 1993/04/05 07:14:30 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:14:30 $";
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
		msgq(sp, M_ERROR, "No window to which to switch");
		return (1);
	} else {
		for (p = sp; p->parent != NULL; p = p->parent);
		sp->snext = p;
	}
	
	F_SET(sp, S_SSWITCH);
	F_SET(sp->snext, S_CUR_INVALID);
	return (0);
}
