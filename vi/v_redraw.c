/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.17 1993/03/25 15:01:21 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:01:21 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>

#include "vi.h"
#include "screen.h"
#include "vcmd.h"

/*
 * v_redraw --
 *	Redraw the screen.
 */
int
v_redraw(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	F_SET(sp, S_REFRESH);
	*rp = *fm;
	return (0);
}
