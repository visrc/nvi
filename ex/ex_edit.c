/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 5.3 1992/04/05 09:23:34 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:23:34 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {EDIT, VISUAL};
static void edit __P((CMDARG *, enum which));

int
ex_edit(cmdp)
	CMDARG *cmdp;
{
	edit(cmdp, EDIT);
	return (0);
}

int
ex_visual(cmdp)
	CMDARG *cmdp;
{
	edit(cmdp, VISUAL);
	return (0);
}

static void
edit(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	long line;
	char **argv, *fname, *start;

	start = fname = NULL;
	argv = cmdp->argv;
	switch(cmdp->argc) {
	case 2:
		start = *argv++;
		if (*start++ != '+')
			goto usage;
		/* FALLTHROUGH */
	case 1:
		fname = *argv;
		break;
	case 0:
		fname = origname;
		break;
	default:
usage:		msg("Usage: edit [+cmd] file.");
		return;
	}

	/*
	 * If ":vi", then switch to visual mode, and if no file is named
	 * then don't switch files.
	 */
	if (cmd == VISUAL) {
		mode = MODE_VI;
		msg("");
		if (cmdp->argc == 0)
			return;
	}

	/* Switch files. */
	if (tmpabort(cmdp->flags & E_FORCE)) {
		tmpstart(fname);
		if (start)
			excmd(start);
		else {
			/*
			 * If editing the previous file, take up where left
			 * off, otherwise start at line 1.
			 */
			line = !strcmp(fname, prevorig) ? prevline : 1;
			if (line <= nlines && line >= 1)
				cursor = MARK_AT_LINE(line);
		}
	} else
		msg("Use edit! to abort changes, or w to save changes.");
}
