/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 10.1 1995/04/13 17:22:14 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:22:14 $";
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

int
ex_mark(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	NEEDFILE(sp, cmdp->cmd);

	if (cmdp->argv[0]->len != 1) {
		msgq(sp, M_ERR, "140|Mark names must be a single character");
		return (1);
	}
	return (mark_set(sp, cmdp->argv[0]->bp[0], &cmdp->addr1, 1));
}
