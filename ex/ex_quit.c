/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 5.9 1992/11/06 18:04:44 bostic Exp $ (Berkeley) $Date: 1992/11/06 18:04:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

int
ex_quit(cmdp)
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	MODIFY_CHECK(curf, force);

	if (!force && file_next(curf, 0)) {
msg("More files; use \":n\" to go to the next file, \":q!\" to quit.");
		return (1);
	}

	if (file_stop(curf, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}

int
ex_wq(cmdp)
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	if (file_sync(curf, force))
		return (1);

	if (!force && file_next(curf, 0)) {
		msg("More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	if (file_stop(curf, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}

int
ex_xit(cmdp)
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	MODIFY_CHECK(curf, force);

	if (file_stop(curf, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}
