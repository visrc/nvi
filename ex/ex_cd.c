/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 5.1 1992/04/02 11:20:45 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:20:45 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_cd(cmdp)
	CMDARG *cmdp;
{
	char *dir;

	switch (cmdp->argc) {
	case 0:
		if ((dir = getenv("HOME")) == NULL) {
			msg("Environment variable HOME not set.");
			return;
		}
		break;
	case 1:
		dir = cmdp->argv[0];
		break;
	default:
		msg("Usage: %s", cmdp->cmd->usage);
		return;
	}

	if (chdir(dir) < 0)
		msg("Error: %s", strerror(errno));
}
