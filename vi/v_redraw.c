/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.13 1993/01/31 10:29:42 bostic Exp $ (Berkeley) $Date: 1993/01/31 10:29:42 $";
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
v_redraw(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	FF_SET(curf, F_REFRESH);
	return (0);
}
