/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 5.16 1993/03/26 13:38:53 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:38:53 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

int
ex_quit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	/* Historic practice: quit! doesn't do autowrite. */
	if (!force)
		MODIFY_CHECK(sp, ep, 0);

	/* Historic practice: quit! doesn't check for other files. */
	if (!force && file_next(sp, ep, 0)) {
		msgq(sp, M_ERROR,
	"More files; use \":n\" to go to the next file, \":q!\" to quit.");
		return (1);
	}

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

int
ex_wq(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	if (file_sync(sp, ep, force))
		return (1);

	if (!force && file_next(sp, ep, 0)) {
		msgq(sp, M_ERROR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	F_SET(sp, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}

int
ex_xit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	int force;

	force = cmdp->flags & E_FORCE;

	MODIFY_CHECK(sp, ep, force);

	F_SET(ep, force ? S_EXIT_FORCE : S_EXIT);
	return (0);
}
