/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.13 1993/03/26 13:39:00 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:39:00 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

int
ex_mark(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (cmdp->argc == 0) {
		msgq(sp, M_ERROR, "No mark name provided.");
		return (1);
	}
	if (cmdp->argv[0][1]) {
		msgq(sp, M_ERROR, "Mark names must be a single character.");
		return (1);
	}
	return (mark_set(sp, ep, cmdp->argv[0][0], &cmdp->addr2));
}
