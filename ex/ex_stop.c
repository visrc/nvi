/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 8.2 1993/09/28 15:21:45 bostic Exp $ (Berkeley) $Date: 1993/09/28 15:21:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

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
