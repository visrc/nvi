/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 8.2 1993/08/05 18:09:57 bostic Exp $ (Berkeley) $Date: 1993/08/05 18:09:57 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *	Edit a file; if none specified, re-edit the current file.
 */
int
ex_edit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;
	FREF *frp;

	switch(cmdp->argc) {
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

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	/* Switch files. */
	if ((tep = file_init(sp, NULL, frp, NULL)) == NULL)
		return (1);
	sp->n_ep = tep;
	sp->n_frp = frp;
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);

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
	FREF *frp;

	switch (cmdp->argc) {
	case 0:
		F_CLR(sp, S_MODE_EX);
		F_SET(sp, S_MODE_VI);
		return (0);
	case 1:
		if ((frp = file_add(sp, sp->frp, cmdp->argv[0], 1)) == NULL)
			return (1);
		set_alt_fname(sp, cmdp->argv[0]);
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	/* Switch files. */
	if ((tep = file_init(sp, NULL, frp, NULL)) == NULL)
		return (1);
	sp->n_ep = tep;
	sp->n_frp = frp;
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);

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
