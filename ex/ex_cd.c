/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 5.8 1992/05/02 09:11:34 bostic Exp $ (Berkeley) $Date: 1992/05/02 09:11:34 $";
#endif /* not lint */

#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_cd -- :cd[!] [directory]
 *	Change directories.
 */
int
ex_cd(cmdp)
	EXCMDARG *cmdp;
{
	char *dir, buf[MAXPATHLEN];

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
	if (getcwd(buf, sizeof(buf)) != NULL)
		msg("New directory: %s", buf);
	return (0);
}
