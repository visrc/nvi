/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_open.c,v 10.2 1995/05/05 18:50:55 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:50:55 $";
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
 * ex_open -- :[line] o[pen] [/pattern/] [flags]
 *
 *	Switch to single line "open" mode.
 *
 * PUBLIC: int ex_open __P((SCR *, EXCMD *));
 */
int
ex_open(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	/* If open option off, disallow open command. */
	if (!O_ISSET(sp, O_OPEN)) {
		msgq(sp, M_ERR,
	    "145|The open command requires that the open option be set");
		return (1);
	}

	msgq(sp, M_ERR, "146|The open command is not yet implemented");
	return (1);
}
