/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.32 1993/02/24 12:53:50 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:53:50 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

/*
 * ex_next -- :next [files]
 *	Edit the next file.
 */
int
ex_next(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *new_ep;

	MODIFY_CHECK(ep, cmdp->flags & E_FORCE);

	if (cmdp->argc) {
		if (file_stop(ep, cmdp->flags & E_FORCE))
			return (1);
		if (file_set(cmdp->argc, (char **)cmdp->argv))
			PANIC;
		if ((new_ep = file_first(1)) == NULL)
			PANIC;
	} else {
		if ((new_ep = file_next(ep, 0)) == NULL) {
			msg(ep, M_ERROR, "No more files to edit.");
			return (1);
		}
		if (file_stop(ep, cmdp->flags & E_FORCE))
			return (1);
	}

	if (file_start(new_ep))
		PANIC;
	return (0);
}

/*
 * ex_prev -- :prev
 *	Edit the previous file.
 */
int
ex_prev(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *prev_ep;

	MODIFY_CHECK(ep, cmdp->flags & E_FORCE);

	if ((prev_ep = file_prev(ep, 0)) == NULL) {
		msg(ep, M_ERROR, "No previous files to edit.");
		return (1);
	}

	if (file_stop(ep, cmdp->flags & E_FORCE))
		return (1);

	if (file_start(prev_ep))
		PANIC;
	return (0);
}

/*
 * ex_rew -- :rew
 *	Edit the first file.
 */
int
ex_rew(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *rew_ep;

	/* Historic practice -- you can rewind to the current file. */
	if ((rew_ep = file_first(0)) == NULL) {
		msg(ep, M_ERROR, "No previous files to rewind.");
		return (1);
	}

	/* Historic practice -- rewind! doesn't do autowrite. */
	if (!(cmdp->flags & E_FORCE))
		MODIFY_CHECK(ep, 0);

	if (file_stop(ep, 0))
		return (1);

	if (file_start(rew_ep))
		PANIC;
	return (0);
}

/*
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	register EXF *list_ep;
	register int cnt, col, sep;
	int len;

	col = len = sep = 0;
	for (cnt = 1, list_ep = file_first(1);
	    list_ep; list_ep = file_next(list_ep, 1)) {
		/*
		 * Ignore files that aren't in the "argument" list unless
		 * they are the one we're currently editing.  I'm not sure
		 * this is right, but the historic vi behavior of not
		 * showing the current file if it was the result of a ":e"
		 * command seems wrong.
		 */
		if (FF_ISSET(list_ep, F_IGNORE) && ep != list_ep)
			continue;
		col += len =
		    strlen(list_ep->name) + sep + (ep == list_ep ? 2 : 0);
		if (col >= SCRCNO(ep) - 1) {
			col = len;
			sep = 0;
			(void)fprintf(ep->stdfp, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)fprintf(ep->stdfp, " ");
		}
		if (ep == list_ep)
			(void)fprintf(ep->stdfp, "[%s]", list_ep->name);
		else
			(void)fprintf(ep->stdfp, "%s", list_ep->name);
		++cnt;
	}
	if (cnt == 1)
		(void)fprintf(ep->stdfp, "No files.\n");
	else
		(void)fprintf(ep->stdfp, "\n");
	return (0);
}
