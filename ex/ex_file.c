/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.15 1992/12/20 15:54:11 bostic Exp $ (Berkeley) $Date: 1992/12/20 15:54:11 $";
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
	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if (!FF_ISSET(curf, F_NONAME))
			free(curf->name);
		else
			FF_CLR(curf, F_NONAME);
		if ((curf->name = strdup((char *)cmdp->argv[0])) == NULL) {
			FF_SET(curf, F_NONAME);
			msg("Error: %s", strerror(errno));
			return (1);
		}
		FF_SET(curf, F_NAMECHANGED);
		break;
	default:
		abort();
	}
	status(curf, cmdp->addr1.lno);
	return (0);
}
