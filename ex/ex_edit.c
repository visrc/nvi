/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.10 1992/04/28 16:55:39 bostic Exp $ (Berkeley) $Date: 1992/04/28 16:55:39 $";
#endif /* not lint */

#include <sys/types.h>
#include <curses.h>
#include <stdio.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
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
	char *fname;

	switch(cmdp->argc) {
	case 0:
		if (cmd == VISUAL)
			return (0);
		fname = origname;
		break;
	case 1:
		fname = cmdp->argv[0];
		break;
	}

	/*
	 * Switch files.  Historic practice is that ex always starts at the
	 * end of the file and vi starts at the beginning, unless a command
	 * is specified.
	 */
	if (tmpabort(cmdp->flags & E_FORCE)) {
		tmpstart(fname);
		if (cmdp->plus)
			(void)ex_cstring(cmdp->plus, strlen(cmdp->plus), 1);
		else {
			if (mode == MODE_VI)
				line = !strcmp(fname, prevorig) ? prevline : 1;
			else
				line = 1;
			if (line <= nlines && line >= 1)
				cursor = MARK_AT_LINE(line);
		}
		return (0);
	}
	msg("%s has been modified but not written.", origname);
	return (1);
}
