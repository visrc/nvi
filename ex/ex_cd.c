/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 5.3 1992/04/05 09:23:30 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:23:30 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_cd(cmdp)
	CMDARG *cmdp;
{
	char *dir;

	switch (cmdp->argc) {
	case 0:
		if ((dir = getenv("HOME")) == NULL) {
			msg("Environment variable HOME not set.");
			return (1);
		}
		break;
	case 1:
		dir = cmdp->argv[0];
		break;
	default:
		msg("Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	if (chdir(dir) < 0) {
		msg("Error: %s", strerror(errno));
		return (1);
	}
	return (0);
}
