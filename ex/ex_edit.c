/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 8.1 1993/06/09 22:23:54 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:23:54 $";
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
		set_altfname(sp, tep->name);
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	/* Switch files. */
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;

	set_altfname(sp, ep->name);

	if (cmdp->plus)
		if ((tep->icommand = strdup(cmdp->plus)) == NULL)
			msgq(sp, M_ERR, "Command not executed: %s",
			    strerror(errno));
		else
			F_SET(tep, F_ICOMMAND);
	return (0);
}

/*
 * ex_visual --	:[line] vi[sual] [file]
 *		:vi[sual] [-^.] [window_size] [flags]
 *	Switch to visual mode.
 *
 * XXX
 * I have no idea what the legal flags are.
 * The second version of this command isn't implemented.
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
		set_altfname(sp, tep->name);
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	/* Switch files. */
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;

	set_altfname(sp, ep->name);

	if (cmdp->plus)
		if ((tep->icommand = strdup(cmdp->plus)) == NULL)
			msgq(sp, M_ERR, "Command not executed: %s",
			    strerror(errno));
		else
			F_SET(tep, F_ICOMMAND);

	F_CLR(sp, S_MODE_EX);
	F_SET(sp, S_MODE_VI);
	return (0);
}
