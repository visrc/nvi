/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.6 1992/04/15 09:13:03 bostic Exp $ (Berkeley) $Date: 1992/04/15 09:13:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {EDIT, VISUAL};

static int edit __P((CMDARG *, enum which));

int
ex_edit(cmdp)
	CMDARG *cmdp;
{
	return (edit(cmdp, EDIT));
}

int
ex_visual(cmdp)
	CMDARG *cmdp;
{
	return (edit(cmdp, VISUAL));
}

static int
edit(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	long line;
	char *fname;

	switch(cmdp->argc) {
	case 0:
		fname = origname;
		break;
	case 1:
		fname = cmdp->argv[0];
		break;
	}

	/*
	 * If ":vi", then switch to visual mode, and if no file is
	 * named, don't switch files.
	 */
	if (cmd == VISUAL) {
		mode = MODE_VI;
		msg("");
		if (cmdp->argc == 0)
			return (0);
	}

	/*
	 * Switch files.  Historic practice is that ex always starts at the
	 * end of the file and vi starts at the beginning, unless a command
	 * is specified.
	 */
	if (tmpabort(cmdp->flags & E_FORCE)) {
		tmpstart(fname);
		if (cmdp->plus)
			(void)ex_cmd(cmdp->plus);
		else {
			if (mode == MODE_VI)
				line = !strcmp(fname, prevorig) ? prevline : 1;
			else
				line = 1;
			if (line <= nlines && line >= 1)
				cursor = MARK_AT_LINE(line);
		}
		refresh();
		return (0);
	}
	msg("The file has been modified but not written.");
	return (1);
}
