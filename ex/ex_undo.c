/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.2 1992/04/04 10:02:48 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:48 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_undo(cmdp)
	CMDARG *cmdp;
{
	undo();
}
