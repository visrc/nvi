/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.9 1992/10/24 14:25:53 bostic Exp $ (Berkeley) $Date: 1992/10/24 14:25:53 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "extern.h"

/*
 * v_undo --
 *	Undoes the last change.
 */
int
v_undo(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
#ifdef NOT_RIGHT_NOW
	if (undo()) {
		scr_ref();
		refresh();
	}
#endif
	*rp = *fm;
	return (0);
}
