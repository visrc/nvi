/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.28 1993/02/25 17:46:32 bostic Exp $ (Berkeley) $Date: 1993/02/25 17:46:32 $";
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

static int edit __P((EXF *, EXCMDARG *, enum which));

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *	Edit a new file.
 */
int
ex_edit(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (edit(ep, cmdp, EDIT));
}

/*
 * ex_visual --	:[line] vi[sual] [type] [count] [flags]
 *	Switch to visual mode.
 */
int
ex_visual(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (edit(ep, cmdp, VISUAL))
		return (1);

	FF_CLR(ep, F_MODE_EX);
	FF_SET(ep, F_MODE_VI);
	return (0);
}

static int
edit(ep, cmdp, cmd)
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
		if ((new_ep = file_locate((char *)cmdp->argv[0])) == NULL) {
			if (file_ins(ep, (char *)cmdp->argv[0], 1) ||
			    (new_ep = file_next(ep, 0)) == NULL)
				return (1);
			FF_SET(new_ep, F_IGNORE);
		} else
			reset = 1;
		break;
	default:
		abort();
	}

	MODIFY_CHECK(ep, cmdp->flags & E_FORCE);

	/* Switch files. */
	FF_SET(ep, cmdp->flags & E_FORCE ? F_SWITCH_FORCE : F_SWITCH);
	ep->enext = new_ep;

	/*
	 * Historic practice is that ex always starts at the end of the file
	 * and vi starts at the beginning, unless a command is specified or
	 * we're going to a file we've edited before.
	 */
	if (!reset)
		if (cmdp->plus || cmd == VISUAL || FF_ISSET(ep, F_MODE_VI))
			SCRLNO(new_ep) = 1;
		else {
			SCRLNO(new_ep) = file_lline(new_ep);
			if (SCRLNO(new_ep) == 0)
				SCRLNO(new_ep) = 1;
		}
	if (cmdp->plus)
		(void)ex_cstring(ep, cmdp->plus, USTRLEN(cmdp->plus), 1);
	return (0);
}
