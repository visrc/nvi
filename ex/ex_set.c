/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 8.2 1993/10/03 17:32:55 bostic Exp $ (Berkeley) $Date: 1993/10/03 17:32:55 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

int
ex_set(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(sp, CHANGED_DISPLAY);
		break;
	default:
		opts_set(sp, cmdp->argv);
		break;
	}
	return (0);
}
