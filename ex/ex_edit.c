/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.32 1993/04/13 16:22:51 bostic Exp $ (Berkeley) $Date: 1993/04/13 16:22:51 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

enum which {EDIT, VISUAL};

static int edit __P((SCR *, EXF *, EXCMDARG *, enum which));

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
	return (edit(sp, ep, cmdp, EDIT));
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
	if (edit(sp, ep, cmdp, VISUAL))
		return (1);

	F_CLR(sp, S_MODE_EX);
	F_SET(sp, S_MODE_VI);
	return (0);
}

static int
edit(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	EXF *tep;
	int reset;

	reset = 0;
	switch(cmdp->argc) {
	case 0:
		if (cmd == VISUAL)
			return (0);
		tep = ep;
		break;
	case 1:
		if ((tep = file_get(sp, ep, (char *)cmdp->argv[0], 1)) == NULL)
			return (1);
		if (F_ISSET(tep, F_NEWSESSION))
			reset = 1;
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, cmdp->flags & E_FORCE);

	/* Switch files. */
	F_SET(sp, cmdp->flags & E_FORCE ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;

	/*
	 * Historic practice is that ex always starts at the end of the file
	 * and vi starts at the beginning, unless a command is specified or
	 * we're going to a file we've edited before.
	 */
	if (!reset)
		if (cmdp->plus || cmd == VISUAL || F_ISSET(sp, S_MODE_VI))
			sp->lno = 1;
		else {
			sp->lno = file_lline(sp, ep);
			if (sp->lno == 0)
				sp->lno = 1;
		}
	if (cmdp->plus)
		(void)ex_cstring(sp, ep, cmdp->plus, strlen(cmdp->plus));
	return (0);
}
