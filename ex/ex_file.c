/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 8.4 1993/11/13 18:02:22 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:02:22 $";
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
	char *p;

	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		if ((p = strdup((char *)cmdp->argv[0])) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
		frp = sp->frp;
		if (F_ISSET(frp, FR_NONAME)) {
			F_CLR(frp, FR_NONAME);
			F_SET(frp, FR_FREE_TNAME);
		} else {
			set_alt_fname(sp, frp->fname);
			FREE(frp->fname, strlen(frp->fname));
		}
		frp->fname = p;

		F_SET(frp, FR_NAMECHANGED);

		/* The read-only bit follows the file name; clear it. */
		F_CLR(frp, FR_RDONLY);
		break;
	default:
		abort();
	}
	status(sp, ep, sp->lno, 1);
	return (0);
}
