/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.16 1993/02/24 12:59:54 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:59:54 $";
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
v_redraw(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	SF_SET(ep, S_REFRESH);
	*rp = *fm;
	return (0);
}
