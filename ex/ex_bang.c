/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 5.1 1992/04/02 11:20:44 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:20:44 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "options.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_bang (:!)
 *	Pass the rest of the line after the character to the program
 *	named by the SHELL environment variable.
 */
void
ex_bang(cmdp)
	CMDARG *cmdp;
{
	static char	prevextra[80];
	char *extra;

	extra = cmdp->argv[0];

	/* if extra is "!", substitute previous command */
	if (*extra == '!')
	{
		if (!*prevextra)
		{
			msg("No previous shell command to substitute for '!'");
			return;
		}
		extra = prevextra;
	}
	else if (strlen(extra) < sizeof(prevextra) - 1)
	{
		strcpy(prevextra, extra);
	}

	/* warn the user if the file hasn't been saved yet */
	if (ISSET(O_WARN) && tstflag(file, MODIFIED))
	{
		if (mode == MODE_VI)
		{
			mode = MODE_COLON;
		}
		msg("Warning: \"%s\" has been modified but not yet saved", origname);
	}

	/* if no lines were specified, just run the command */
	suspend_curses();
	if (cmdp->addr1 == 0L)
	{
		system(extra);
	}
	else /* pipe lines from the file through the command */
	{
		filter(cmdp->addr1, cmdp->addr2, extra);
	}

	/* resume curses quietly for MODE_EX, but noisily otherwise */
	resume_curses(mode == MODE_EX);
}

