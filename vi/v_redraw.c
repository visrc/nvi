/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.5 1992/05/15 11:14:18 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:18 $";
#endif /* not lint */

#include <sys/types.h>
#include <curses.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_redraw --
 *	Redraw the screen.
 */
int
v_redraw(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	wrefresh(curscr);
	return (1);
}
