/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 5.13 1992/12/05 11:08:28 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:28 $";
#endif /* not lint */

#include <sys/param.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_cd -- :cd[!] [directory]
 *	Change directories.
 */
int
ex_cd(cmdp)
	EXCMDARG *cmdp;
{
	char *dir, buf[MAXPATHLEN];

	switch (cmdp->argc) {
	case 0:
		if ((dir = getenv("HOME")) == NULL) {
			msg("Environment variable HOME not set.");
			return (1);
		}
		break;
	case 1:
		dir = (char *)cmdp->argv[0];
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
