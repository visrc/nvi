/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 8.4 1993/12/02 10:47:23 bostic Exp $ (Berkeley) $Date: 1993/12/02 10:47:23 $";
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
	if (cmdp->argv[0]->len != 1) {
		msgq(sp, M_ERR, "Mark names must be a single character.");
		return (1);
	}
	return (mark_set(sp, ep, cmdp->argv[0]->bp[0], &cmdp->addr1, 1));
}
