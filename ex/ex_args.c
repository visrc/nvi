/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.40 1993/05/05 10:57:37 bostic Exp $ (Berkeley) $Date: 1993/05/05 10:57:37 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_next -- :next [files]
 *	Edit the next file.
 */
int
ex_next(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	if (cmdp->argc) {
		/* Mark all the current files as ignored. */
		for (tep = ep; tep->prev; tep = tep->prev)
			F_SET(tep, F_IGNORE);
		for (tep = ep; tep->next; tep = tep->next)
			F_SET(tep, F_IGNORE);
		F_SET(ep, F_IGNORE);

		/* Add the new files into the file list. */
		if (file_set(sp, cmdp->argc, (char **)cmdp->argv))
			return (1);
		
		/* Get the next file to edit. */
		if ((tep = file_first(sp, 1)) == NULL)
			return (1);
	} else if ((tep = file_next(sp, ep, 0)) == NULL) {
		msgq(sp, M_ERR, "No more files to edit.");
		return (1);
	}

	sp->enext = tep;
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
	return (0);
}

/*
 * ex_prev -- :prev
 *	Edit the previous file.
 */
int
ex_prev(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	if ((tep = sp->eprev) == NULL) {
		msgq(sp, M_ERR, "No previous files to edit.");
		return (1);
	}

	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;
	return (0);
}

/*
 * ex_rew -- :rew
 *	Edit the first file.
 */
int
ex_rew(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;

	/* Historic practice -- you can rewind to the current file. */
	if ((tep = file_first(sp, 0)) == NULL) {
		msgq(sp, M_ERR, "No previous files to rewind.");
		return (1);
	}

	/* Historic practice -- rewind! doesn't do autowrite. */
	if (!F_ISSET(cmdp, E_FORCE))
		MODIFY_CHECK(sp, ep, 0);

	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
	sp->enext = tep;
	return (0);
}

/*
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;
	int cnt, col, len, sep;

	col = len = sep = 0;
	for (cnt = 1, tep = file_first(sp, 1);
	    tep != NULL; tep = file_next(sp, tep, 1)) {
		/*
		 * Ignore files that aren't in the "argument" list unless
		 * they are the one we're currently editing.  I'm not sure
		 * this is right, but the historic vi behavior of not
		 * showing the current file if it was the result of a ":e"
		 * command seems wrong.
		 */
		if (F_ISSET(tep, F_IGNORE) && ep != tep)
			continue;
		col += len = tep->nlen + sep + (ep == tep ? 2 : 0);
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)fprintf(sp->stdfp, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)fprintf(sp->stdfp, " ");
		}
		if (ep == tep)
			(void)fprintf(sp->stdfp, "[%s]", tep->name);
		else
			(void)fprintf(sp->stdfp, "%s", tep->name);
		++cnt;
	}
	if (cnt == 1)
		(void)fprintf(sp->stdfp, "No files.\n");
	else
		(void)fprintf(sp->stdfp, "\n");
	return (0);
}
