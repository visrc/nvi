/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.18 1993/02/28 14:00:33 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:33 $";
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
ex_file(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	char *p;

	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if ((p = strdup((char *)cmdp->argv[0])) == NULL) {
			ep->msg(ep, M_ERROR, "Error: %s", strerror(errno));
			return (1);
		}
		free(ep->name);
		ep->name = p;
		FF_CLR(ep, F_NONAME);
		FF_SET(ep, F_NAMECHANGED);
		break;
	default:
		abort();
	}
	status(ep, cmdp->addr1.lno);
	return (0);
}
