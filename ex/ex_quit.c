/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_quit.c,v 5.7 1992/10/10 13:57:03 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:57:03 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

enum which {QUIT, WQ, XIT};
static int quit __P((EXCMDARG *, enum which));

int
ex_quit(cmdp)
	EXCMDARG *cmdp;
{
	return (quit(cmdp, QUIT));
}

int
ex_wq(cmdp)
	EXCMDARG *cmdp;
{
	return (quit(cmdp, WQ));
}

int
ex_xit(cmdp)
	EXCMDARG *cmdp;
{
	return (quit(cmdp, XIT));
}

/*
 * quit --
 *	Quit the file.
 */
static int
quit(cmdp, cmd)
	EXCMDARG *cmdp;
	enum which cmd;
{
	static int whenwarned;
	int oldflag;

	/* If there are more files to edit, then warn user. */
	if (file_next(curf) &&
	    whenwarned != changes && (!cmdp->flags & E_FORCE || cmd != QUIT)) {
		msg("More files to edit -- Use \":n\" to go to next file");
		whenwarned = changes;
		return;
	}

	switch(cmd) {
	case WQ:
		if (file_sync(curf, 0))
			return (1);
		/* FALLTHROUGH */
	case QUIT:
	case XIT:
		if (file_stop(curf, cmdp->flags & E_FORCE))
			return (1);
	}
	mode = MODE_QUIT;
	return (0);
}
