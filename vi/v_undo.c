/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.5 1992/05/07 12:49:41 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:41 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_undo --
 *	Undoes the last change.
 */
/* ARGSUSED */
MARK *
v_undo(m)
	MARK *m;
{
#ifdef NOT_RIGHT_NOW
	if (undo())
		scr_ref();
#endif
	return (&cursor);
}
