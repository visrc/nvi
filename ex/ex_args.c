/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.25 1992/11/07 13:41:46 bostic Exp $ (Berkeley) $Date: 1992/11/07 13:41:46 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"
#include "extern.h"

/*
 * ex_next -- :next [files]
 *	Edit the next file.
 */
int
ex_next(cmdp)
	EXCMDARG *cmdp;
{
	EXF *ep;

	MODIFY_CHECK(curf, cmdp->flags & E_FORCE);

	if (cmdp->argc) {
		if (file_stop(curf, cmdp->flags & E_FORCE))
			return (1);
		if (file_set(cmdp->argc, (char **)cmdp->argv))
			PANIC;
		if ((ep = file_first(1)) == NULL)
			PANIC;
	} else if ((ep = file_next(curf, 0)) == NULL) {
		msg("No more files to edit.");
		return (1);
	}

	if (file_start(ep))
		PANIC;
	return (0);
}

/*
 * ex_prev -- :prev
 *	Edit the previous file.
 */
int
ex_prev(cmdp)
	EXCMDARG *cmdp;
{
	EXF *ep;

	MODIFY_CHECK(curf, cmdp->flags & E_FORCE);

	if ((ep = file_prev(curf, 0)) == NULL) {
		msg("No previous files to edit.");
		return (1);
	}

	if (file_stop(curf, cmdp->flags & E_FORCE))
		return (1);

	if (file_start(ep))
		PANIC;
	return (0);
}

/*
 * ex_rew -- :rew
 *	Edit the first file.
 */
int
ex_rew(cmdp)
	EXCMDARG *cmdp;
{
	EXF *ep;

	MODIFY_CHECK(curf, cmdp->flags & E_FORCE);

	if (file_prev(curf, 0) == NULL) {
		msg("No previous files to rewind.");
		return (1);
	}
	if ((ep = file_first(0)) == NULL) {
		msg("No previous files to rewind.");
		return (1);
	}
	
	if (file_stop(curf, 0))
		return (1);

	if (file_start(ep))
		PANIC;
	return (0);
}

/*
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(cmdp)
	EXCMDARG *cmdp;
{
	register EXF *ep;
	register int cnt, col, sep;
	int len;

	col = len = sep = 0;
	for (cnt = 1, ep = file_first(1); ep; ep = file_next(ep, 1)) {
		/*
		 * Ignore files that aren't in the "argument" list unless
		 * they are the one we're currently editing.  I'm not sure
		 * this is right, but the historic vi behavior of not
		 * showing the current file if it was the result of a ":e"
		 * command seems wrong.
		 */
		if (FF_ISSET(ep, F_IGNORE) && curf != ep)
			continue;
		col += len = strlen(ep->name) + sep + (curf == ep ? 2 : 0);
		if (col >= curf->cols - 1) {
			col = len;
			sep = 0;
			(void)fprintf(curf->stdfp, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)fprintf(curf->stdfp, " ");
		}
		if (curf == ep)
			(void)fprintf(curf->stdfp, "[%s]", ep->name);
		else
			(void)fprintf(curf->stdfp, "%s", ep->name);
		++cnt;
	}
	if (cnt == 1)
		(void)fprintf(curf->stdfp, "No files.\n");
	else
		(void)fprintf(curf->stdfp, "\n");
	return (0);
}
