/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.29 1993/03/25 14:59:47 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:47 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"

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
	EXF *new_ep;
	int reset;

	reset = 0;
	switch(cmdp->argc) {
	case 0:
		if (cmd == VISUAL)
			return (0);
		new_ep = ep;
		break;
	case 1:
		if ((new_ep =
		    file_locate(sp, (char *)cmdp->argv[0])) == NULL) {
			if (file_ins(sp, ep, (char *)cmdp->argv[0], 1) ||
			    (new_ep = file_next(sp, ep, 0)) == NULL)
				return (1);
			F_SET(new_ep, F_IGNORE);
		} else
			reset = 1;
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, cmdp->flags & E_FORCE);

	/* Switch files. */
	F_SET(sp, cmdp->flags & E_FORCE ? S_SWITCH_FORCE : S_SWITCH);
	sp->enext = new_ep;

	/*
	 * Historic practice is that ex always starts at the end of the file
	 * and vi starts at the beginning, unless a command is specified or
	 * we're going to a file we've edited before.
	 */
	if (!reset)
		if (cmdp->plus || cmd == VISUAL || F_ISSET(sp, S_MODE_VI))
			ep->lno = 1;
		else {
			ep->lno = file_lline(sp, ep);
			if (ep->lno == 0)
				ep->lno = 1;
		}
	if (cmdp->plus)
		(void)ex_cstring(sp, ep, cmdp->plus, USTRLEN(cmdp->plus), 1);
	return (0);
}
