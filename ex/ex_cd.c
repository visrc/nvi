/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_cd.c,v 8.4 1994/03/08 19:39:13 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:39:13 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

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
		dir = cmdp->argv[0]->bp;
		break;
	default:
		abort();
	}

	if (chdir(dir) < 0) {
		msgq(sp, M_SYSERR, dir);
		return (1);
	}
	if (getcwd(buf, sizeof(buf)) != NULL)
		msgq(sp, M_INFO, "New directory: %s", buf);
	return (0);
}
