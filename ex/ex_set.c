/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 5.4 1992/04/19 08:54:00 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:54:00 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_set(cmdp)
	EXCMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(0);
		break;
	default:
		opts_set(cmdp->argv);
		break;
	}
	return (0);
}
