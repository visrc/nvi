/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 5.2 1993/02/24 12:48:28 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:48:28 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_stop -- :stop
 *	Suspend execution.
 */
int
ex_stop(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (kill(0, SIGTSTP)) {
		msg(ep, M_ERROR, "Error: SIGTSTP: %s", strerror(errno));
		return (1);
	}
	return (0);
}
