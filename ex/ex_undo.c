/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.5 1992/04/19 08:54:05 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:54:05 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_undo(cmdp)
	EXCMDARG *cmdp;
{
	undo();
	
	autoprint = 1;
	return (0);
}
