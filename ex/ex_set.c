/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 5.10 1993/03/26 13:39:09 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:39:09 $";
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
		opts_dump(sp, 0);
		break;
	default:
		opts_set(sp, cmdp->argv);
		break;
	}
	return (0);
}
