/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 5.8 1992/10/26 09:08:22 bostic Exp $ (Berkeley) $Date: 1992/10/26 09:08:22 $";
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

	if (file_modify(curf, force))
		return (1);
	if (!force && file_next(curf)) {
		msg("More files to edit; use \":n\" to go to the next file");
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

	if (file_sync(curf, 0))
		return (1);
	if (!force && file_next(curf)) {
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

	if (file_modify(curf, force))
		return (1);
	if (file_stop(curf, force))
		return (1);
	mode = MODE_QUIT;
	return (0);
}
