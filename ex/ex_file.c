/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.9 1992/10/18 13:07:45 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:07:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_file(cmdp)
	EXCMDARG *cmdp;
{
	recno_t lline;

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

	lline = file_lline(curf);
	msg("\"%s\" %s%s %ld lines,  line %ld [%ld%%]",
	    curf->name,
	    curf->flags & F_MODIFIED ? "[MODIFIED]" : "[UNMODIFIED]",
	    curf->flags & F_RDONLY ? "[READONLY]" : "", lline,
	    cmdp->addr1.lno, cmdp->addr1.lno * 100 / lline);
	return (0);
}
