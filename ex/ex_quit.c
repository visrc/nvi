/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 9.1 1994/11/09 18:40:59 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:40:59 $";
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

#include "vi.h"
#include "excmd.h"

/*
 * ex_quit -- :quit[!]
 *	Quit.
 */
int
ex_quit(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	int force;

	force = F_ISSET(cmdp, E_FORCE);

	/* Check for file modifications, or more files to edit. */
	if (file_m2(sp, force) || ex_ncheck(sp, force))
		return (1);

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}
