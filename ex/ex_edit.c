/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 8.9 1993/09/29 16:16:58 bostic Exp $ (Berkeley) $Date: 1993/09/29 16:16:58 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *		:vi[sual][!] [+cmd] [file]
 *
 *	Edit a file; if none specified, re-edit the current file.
 *	The second form of the command can only be executed while
 *	in vi mode.  See the hack in ex.c:ex_cmd().
 */
int
ex_edit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	FREF *frp;

	switch (cmdp->argc) {
	case 0:
		frp = sp->frp;
		break;
	case 1:
		if ((frp = file_add(sp, sp->frp, cmdp->argv[0], 1)) == NULL)
			return (1);
		set_alt_fname(sp, cmdp->argv[0]);
		break;
	default:
		abort();
	}

	/*
	 * Check for modifications.
	 *
	 * !!!
	 * Contrary to POSIX 1003.2-1992, autowrite did not affect :edit.
	 */
	if (F_ISSET(ep, F_MODIFIED) &&
	    ep->refcnt <= 1 && !F_ISSET(cmdp, E_FORCE)) {
		msgq(sp, M_ERR,
		    "Modified since last write; write or use ! to override.");
		return (1);
	}

	/* Switch files. */
	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);
	F_SET(sp, S_FSWITCH);
	return (0);
}
