/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 5.4 1992/04/05 18:58:47 bostic Exp $ (Berkeley) $Date: 1992/04/05 18:58:47 $";
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

	/*
	 * If the file has been modified, write a warning and fail, unless
	 * overridden by the '!' flag.
	 */
	if (!(cmdp->flags & E_FORCE) && tstflag(file, MODIFIED)) {
		msg("The file has been modified but not written.");
		return (1);
	}

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
	}

	if (chdir(dir) < 0) {
		msg("%s: %s", dir, strerror(errno));
		return (1);
	}
	return (0);
}
