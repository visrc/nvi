/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.2 1992/04/22 08:10:28 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:28 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "curses.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_redraw --
 *	Redraw the screen.
 */
MARK
v_redraw()
{
	iredraw();
	return (cursor);
}
