/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 5.8 1993/02/16 20:10:22 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:22 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"

int
ex_set(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(ep, 0);
		break;
	default:
		opts_set(ep, cmdp->argv);
		break;
	}
	return (0);
}
