/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.14 1993/02/16 20:08:42 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:08:42 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_redraw --
 *	Redraw the screen.
 */
int
v_redraw(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	FF_SET(ep, F_REFRESH);
	return (0);
}
