/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 5.11 1993/02/16 20:10:12 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:12 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"

int
ex_quit(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	MODIFY_CHECK(ep, force);

	if (!force && file_next(ep, 0)) {
		msg(ep, M_ERROR,
"More files; use \":n\" to go to the next file, \":q!\" to quit.");
		return (1);
	}

	if (file_stop(ep, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}

int
ex_wq(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	if (file_sync(ep, force))
		return (1);

	if (!force && file_next(ep, 0)) {
		msg(ep, M_ERROR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	if (file_stop(ep, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}

int
ex_xit(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	MODIFY_CHECK(ep, force);

	if (file_stop(ep, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}
