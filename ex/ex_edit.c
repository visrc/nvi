/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.12 1992/05/07 12:46:38 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:46:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "exf.h"
#include "extern.h"

enum which {EDIT, VISUAL};

static int edit __P((EXCMDARG *, enum which));

int
ex_edit(cmdp)
	EXCMDARG *cmdp;
{
	return (edit(cmdp, EDIT));
}

int
ex_visual(cmdp)
	EXCMDARG *cmdp;
{
	if (edit(cmdp, VISUAL))
		return (1);

	/* Switch to visual mode. */
	mode = MODE_VI;
	return (0);
}

static int
edit(cmdp, cmd)
	EXCMDARG *cmdp;
	enum which cmd;
{
	long line;
	EXF *ep;
	int previous;
	char *fname;

	switch(cmdp->argc) {
	case 0:
		if (cmd == VISUAL)
			return (0);
		break;
	case 1:
		if ((ep = file_prev(curf)) != NULL &&
		    !strcmp(ep->name, cmdp->argv[0]))
			previous = 1;
		else {
			previous = 0;
			if (file_ins(curf, cmdp->argv[0]) ||
			    (ep = file_next(curf)) == NULL)
			return (1);
		}
		break;
	}

	/*
	 * Switch files.  Historic practice is that ex always starts at the
	 * end of the file and vi starts at the beginning, unless a command
	 * is specified.
	 */
	if (file_stop(curf, cmdp->flags & E_FORCE))
		return (1);
	if (file_start(curf))
		return (1);

	if (cmdp->plus)
		(void)ex_cstring(cmdp->plus, strlen(cmdp->plus), 1);
	else {
		line = mode == MODE_VI ? ep->lno : 1;
		if (line <= nlines && line >= 1)
			cursor.lno = line;
	}
	return (0);
}
