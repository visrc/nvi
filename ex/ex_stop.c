/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 9.3 1995/01/11 16:15:55 bostic Exp $ (Berkeley) $Date: 1995/01/11 16:15:55 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../sex/sex_screen.h"

/*
 * ex_stop -- :stop[!]
 *	      :suspend[!]
 *	Suspend execution.
 */
int
ex_stop(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	/* For some strange reason, the force flag turns off autowrite. */
	if (!F_ISSET(cmdp, E_FORCE) && file_aw(sp, FS_ALL))
		return (1);

	return (sp->s_suspend(sp));
}
