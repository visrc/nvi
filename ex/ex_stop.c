/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 8.5 1994/03/08 19:39:45 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:39:45 $";
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
#include "sex/sex_screen.h"

/*
 * ex_stop -- :stop[!]
 *	      :suspend[!]
 *	Suspend execution.
 */
int
ex_stop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* For some strange reason, the force flag turns off autowrite. */
	if (F_ISSET(ep, F_MODIFIED) && O_ISSET(sp, O_AUTOWRITE) &&
	    !F_ISSET(cmdp, E_FORCE)) {
		if (file_write((sp), (ep), NULL, NULL, NULL, FS_ALL))
			return (1);
		if (sex_refresh(sp, ep))
			return (1);
	}
	return (sp->s_suspend(sp));
}
