/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.6 1992/05/15 11:08:27 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:08:27 $";
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
		msg("No mark name; use 'a' to 'z'.");
		return (1);
	}
	if (cmdp->argv[1]) {
		msg("Invalid mark name; use 'a' to 'z'.");
		return (1);
	}
	return (mark_set(cmdp->argv[0], &cmdp->addr2));
}
