/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.11 1993/02/28 14:00:38 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:38 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

int
ex_mark(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (cmdp->argc == 0) {
		ep->msg(ep, M_ERROR, "No mark name provided.");
		return (1);
	}
	if (cmdp->argv[0][1]) {
		ep->msg(ep, M_ERROR, "Mark names must be a single character.");
		return (1);
	}
	return (mark_set(ep, cmdp->argv[0][0], &cmdp->addr2));
}
