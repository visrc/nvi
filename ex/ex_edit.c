/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.26 1993/02/16 20:10:09 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:09 $";
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

	mode = MODE_VI;
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

	/* Switch files. */
	MODIFY_CHECK(ep, cmdp->flags & E_FORCE);
	if (file_stop(ep, cmdp->flags & E_FORCE))
		return (1);
	if (file_start(new_ep))
		PANIC;

	/*
	 * Historic practice is that ex always starts at the end of the file
	 * and vi starts at the beginning, unless a command is specified or
	 * we're going to a file we've edited before.
	 */
	if (!reset)
		if (cmdp->plus || cmd == VISUAL || mode == MODE_VI)
			new_ep->lno = 1;
		else {
			new_ep->lno = file_lline(new_ep);
			if (new_ep->lno == 0)
				new_ep->lno = 1;
		}
	if (cmdp->plus)
		(void)ex_cstring(ep, cmdp->plus, USTRLEN(cmdp->plus), 1);
		/*
		 * XXX
		 * THE UNDERLYING EXF MAY HAVE CHANGED (but we don't care).
		 */
	return (0);
}
