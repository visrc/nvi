/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.25 1993/05/14 16:38:50 bostic Exp $ (Berkeley) $Date: 1993/05/14 16:38:50 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_file -- :f[ile] [name]
 *	Status line and change the file's name.
 */
int
ex_file(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	char *p;

	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if ((p = strdup((char *)cmdp->argv[0])) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			return (1);
		}
		if (F_ISSET(ep, F_NONAME))
			F_CLR(ep, F_NONAME);
		else {
			if (ex_set_altfname(sp, ep->name))
				return (1);
			free(ep->name);
		}
		ep->name = p;
		F_SET(ep, F_NAMECHANGED);
		break;
	default:
		abort();
	}
	status(sp, ep, sp->lno, 1);
	return (0);
}
