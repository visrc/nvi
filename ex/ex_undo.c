/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.4 1992/04/18 09:54:23 bostic Exp $ (Berkeley) $Date: 1992/04/18 09:54:23 $";
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
	
	autoprint = 1;
	return (0);
}
