/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 10.2 1995/05/05 18:51:46 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:51:46 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

/*
 * ex_set -- :set
 *	Ex set option.
 *
 * PUBLIC: int ex_set __P((SCR *, EXCMD *));
 */
int
ex_set(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(sp, CHANGED_DISPLAY);
		break;
	default:
		if (opts_set(sp, cmdp->argv, 0, cmdp->cmd->usage))
			return (1);
		break;
	}
	return (0);
}
