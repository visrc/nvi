/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 5.19 1993/04/12 14:35:38 bostic Exp $ (Berkeley) $Date: 1993/04/12 14:35:38 $";
#endif /* not lint */

#include <sys/param.h>

#include <errno.h>
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
ex_cd(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	char *dir, buf[MAXPATHLEN];

	switch (cmdp->argc) {
	case 0:
		if ((dir = getenv("HOME")) == NULL) {
			msgq(sp, M_ERR,
			    "Environment variable HOME not set.");
			return (1);
		}
		break;
	case 1:
		dir = (char *)cmdp->argv[0];
		break;
	default:
		abort();
	}

	if (chdir(dir) < 0) {
		msgq(sp, M_ERR, "%s: %s", dir, strerror(errno));
		return (1);
	}
	if (getcwd(buf, sizeof(buf)) != NULL)
		msgq(sp, M_INFO, "New directory: %s", buf);
	return (0);
}
