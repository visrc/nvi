/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.16 1993/02/15 14:13:49 bostic Exp $ (Berkeley) $Date: 1993/02/15 14:13:49 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

int
ex_file(cmdp)
	EXCMDARG *cmdp;
{
	char *p;

	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if ((p = strdup((char *)cmdp->argv[0])) == NULL) {
			msg("Error: %s", strerror(errno));
			return (1);
		}
		free(curf->name);
		curf->name = p;
		FF_CLR(curf, F_NONAME);
		FF_SET(curf, F_NAMECHANGED);
		break;
	default:
		abort();
	}
	status(curf, cmdp->addr1.lno);
	return (0);
}
