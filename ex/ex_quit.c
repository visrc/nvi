/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 5.4 1992/04/19 08:53:49 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:53:49 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

enum which {QUIT, WQ, XIT};
static void quit __P((EXCMDARG *, enum which));

int
ex_quit(cmdp)
	EXCMDARG *cmdp;
{
	quit(cmdp, QUIT);
	return (0);
}

int
ex_wq(cmdp)
	EXCMDARG *cmdp;
{
	quit(cmdp, WQ);
	return (0);
}

int
ex_xit(cmdp)
	EXCMDARG *cmdp;
{
	quit(cmdp, XIT);
	return (0);
}

/* also called from :wq -- always writes back in this case */

static void
quit(cmdp, cmd)
	EXCMDARG *cmdp;
	enum which cmd;
{
	static long	whenwarned;	/* when the user was last warned of extra files */
	int		oldflag;

	/* if there are more files to edit, then warn user */
	if (file_cnt() && whenwarned != changes && (!cmdp->flags & E_FORCE || cmd != QUIT))
	{
		msg("More files to edit -- Use \":n\" to go to next file");
		whenwarned = changes;
		return;
	}

	if (cmd == QUIT)
	{
		oldflag = ISSET(O_AUTOWRITE);
		UNSET(O_AUTOWRITE);
		if (tmpabort(cmdp->flags & E_FORCE))
		{
			mode = MODE_QUIT;
		}
		else
		{
			msg("Use q! to abort changes, or wq to save changes");
		}
		if (oldflag)
			SET(O_AUTOWRITE);
	}
	else
	{
		/* else try to save this file */
		oldflag = tstflag(file, MODIFIED);
		if (cmd == WQ)
			setflag(file, MODIFIED);
		if (tmpend(cmdp->flags & E_FORCE))
		{
			mode = MODE_QUIT;
		}
		else
		{
			msg("Could not save file -- use quit! to abort changes, or w filename");
		}
		if (!oldflag)
			clrflag(file, MODIFIED);
	}
}
