/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.23 1992/11/07 13:41:56 bostic Exp $ (Berkeley) $Date: 1992/11/07 13:41:56 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

enum which {EDIT, VISUAL};

static int edit __P((EXCMDARG *, enum which));

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *	Edit a new file.
 */
int
ex_edit(cmdp)
	EXCMDARG *cmdp;
{
	return (edit(cmdp, EDIT));
}

/*
 * ex_visual --	:[line] vi[sual] [type] [count] [flags]
 *	Switch to visual mode.
 */
int
ex_visual(cmdp)
	EXCMDARG *cmdp;
{
	if (edit(cmdp, VISUAL))
		return (1);

	mode = MODE_VI;
	return (0);
}

static int
edit(cmdp, cmd)
	EXCMDARG *cmdp;
	enum which cmd;
{
	EXF *ep;
	int reset;

	reset = 0;
	switch(cmdp->argc) {
	case 0:
		if (cmd == VISUAL)
			return (0);
		ep = curf;
		break;
	case 1:
		if ((ep = file_locate((char *)cmdp->argv[0])) == NULL) {
			if (file_ins(curf, (char *)cmdp->argv[0], 1) ||
			    (ep = file_next(curf, 0)) == NULL)
				return (1);
			FF_SET(ep, F_IGNORE);
		} else
			reset = 1;
		break;
	}

	/* Switch files. */
	MODIFY_CHECK(curf, cmdp->flags & E_FORCE);
	if (file_stop(curf, cmdp->flags & E_FORCE))
		return (1);
	if (file_start(ep))
		PANIC;

	/*
	 * Historic practice is that ex always starts at the end of the file
	 * and vi starts at the beginning, unless a command is specified or
	 * we're going to a file we've edited before.
	 */
	if (!reset)
		if (cmdp->plus || cmd == VISUAL || mode == MODE_VI)
			curf->lno = 1;
		else {
			curf->lno = file_lline(curf);
			if (curf->lno == 0)
				curf->lno = 1;
		}
	if (cmdp->plus)
		(void)ex_cstring(cmdp->plus, USTRLEN(cmdp->plus), 1);
	return (0);
}
