/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 5.1 1992/04/02 11:21:10 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:10 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_set(cmdp)
	CMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(0);
		break;
	default:
		opts_set(cmdp->argv);
		break;
	}
}
