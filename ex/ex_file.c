/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.11 1992/11/02 22:18:25 bostic Exp $ (Berkeley) $Date: 1992/11/02 22:18:25 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_file(cmdp)
	EXCMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if (!(curf->flags & F_NONAME))
			free(curf->name);
		else
			curf->flags &= ~F_NONAME;
		if ((curf->name = strdup((char *)cmdp->argv[0])) == NULL) {
			curf->flags |= F_NONAME;
			msg("Error: %s", strerror(errno));
			return (1);
		}
		break;
	default:
		msg("Usage: file [newname].");
		return (1);
	}
	status(curf, cmdp->addr1.lno);
	return (0);
}
