/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.7 1992/10/10 13:56:44 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:56:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_undo(cmdp)
	EXCMDARG *cmdp;
{
#ifdef NOT_RIGHT_NOW
	undo();
#endif
	
	autoprint = 1;
	return (0);
}
