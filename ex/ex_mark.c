/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.7 1992/05/21 12:54:46 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:54:46 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_mark(cmdp)
	EXCMDARG *cmdp;
{
	if (cmdp->argc == 0) {
		msg("No mark name provided.");
		return (1);
	}
	if (cmdp->argv[0][1]) {
		msg("Mark names must be a single character.");
		return (1);
	}
	return (mark_set(cmdp->argv[0][0], &cmdp->addr2));
}
