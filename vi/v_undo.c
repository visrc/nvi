/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.3 1992/05/02 09:33:27 bostic Exp $ (Berkeley) $Date: 1992/05/02 09:33:27 $";
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
MARK
v_undo(m)
	MARK m;
{
	if (undo())
		scr_ref();
	return (cursor);
}
