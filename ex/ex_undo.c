/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.3 1992/04/05 09:23:54 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:23:54 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_undo(cmdp)
	CMDARG *cmdp;
{
	undo();
	return (0);
}
