/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 5.9 1993/05/11 16:10:49 bostic Exp $ (Berkeley) $Date: 1993/05/11 16:10:49 $";
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
 *	Suspend execution.
 */
int
ex_stop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* For some strange reason, the force flag turns off autowrite. */
	if (!F_ISSET(cmdp, E_FORCE))
		AUTOWRITE(sp, ep);

	return (sp->s_suspend(sp));
}
