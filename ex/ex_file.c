/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 8.7 1993/12/02 10:47:41 bostic Exp $ (Berkeley) $Date: 1993/12/02 10:47:41 $";
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
	FREF *frp;
	char *p, *t;

	switch (cmdp->argc) {
	case 0:
		break;
	case 1:
		frp = sp->frp;

		/* Make sure can allocate enough space. */
		if ((p = strdup(cmdp->argv[0]->bp)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}

		/* If already have a file name, it becomes the alternate. */
		if ((t = FILENAME(frp)) != NULL)
			set_alt_name(sp, t);

		/* Free any previously changed name. */
		if (frp->cname != NULL)
			free(frp->cname);
		frp->cname = p;

		/* The read-only bit follows the file name; clear it. */
		F_CLR(frp, FR_RDONLY);

		/* Have to force a write if the file exists, next time. */
		F_CLR(frp, FR_CHANGEWRITE);
		break;
	default:
		abort();
	}
	status(sp, ep, sp->lno, 1);
	return (0);
}
