/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.4 1992/05/07 12:49:11 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:11 $";
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
MARK *
v_redraw()
{
	wrefresh(curscr);
	return (&cursor);
}
