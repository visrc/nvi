/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.33 1993/04/17 11:57:13 bostic Exp $ (Berkeley) $Date: 1993/04/17 11:57:13 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *	Edit a new file.
 */
int
ex_edit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;

	switch(cmdp->argc) {
	case 0:
		tep = ep;
		break;
	case 1:
		if ((tep = file_get(sp, ep, (char *)cmdp->argv[0], 1)) == NULL)
			return (1);
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, cmdp->flags & E_FORCE);

	/* Switch files. */
	F_SET(sp, cmdp->flags & E_FORCE ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;

	if (cmdp->plus)
		if ((tep->icommand = strdup(cmdp->plus)) == NULL)
			msgq(sp, M_ERR, "Command not executed: %s",
			    strerror(errno));
		else
			F_SET(tep, F_ICOMMAND);
	return (0);
}

/*
 * ex_visual --	:[line] vi[sual] [type] [count] [flags]
 *	Switch to visual mode.
 */
int
ex_visual(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;

	switch (cmdp->argc) {
	case 0:
		return (0);
	case 1:
		if ((tep = file_get(sp, ep, (char *)cmdp->argv[0], 1)) == NULL)
			return (1);
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, cmdp->flags & E_FORCE);

	/* Switch files. */
	F_SET(sp, cmdp->flags & E_FORCE ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;

	F_CLR(sp, S_MODE_EX);
	F_SET(sp, S_MODE_VI);
	return (0);
}
