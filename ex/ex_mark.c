/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.9 1992/12/05 11:08:41 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

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
