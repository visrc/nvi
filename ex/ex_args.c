/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 8.2 1993/08/05 18:10:38 bostic Exp $ (Berkeley) $Date: 1993/08/05 18:10:38 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_next -- :next [files]
 *	Edit the next file, optionally setting the list of files.
 */
int
ex_next(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	FREF *frp;
	char **argv;

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	if (cmdp->argc) {
		/* Mark all the current files as ignored. */
		for (frp = sp->frefhdr.next;
		    frp != (FREF *)&sp->frefhdr; frp = frp->next)
			F_SET(frp, FR_IGNORE);

		/* Add the new files into the file list. */
		for (argv = cmdp->argv; *argv != NULL; ++argv)
			if (file_add(sp, NULL, *argv, 0) == NULL)
				return (1);
		
		if ((frp = file_first(sp, 0)) == NULL)
			return (1);
	} else if ((frp = file_next(sp, 0)) == NULL) {
		msgq(sp, M_ERR, "No more files to edit.");
		return (1);
	}

	if ((sp->n_ep = file_init(sp, NULL, frp, NULL)) == NULL)
		return (1);
	sp->n_frp = frp;
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
	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	if (sp->p_frp == NULL) {
		msgq(sp, M_ERR, "No previous files to edit.");
		return (1);
	}

	if ((sp->n_ep = file_init(sp, NULL, sp->p_frp, NULL)) == NULL)
		return (1);
	sp->n_frp = sp->p_frp;
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
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
	FREF *frp, *tfrp;

	/* Historic practice -- you can rewind to the current file. */
	if ((frp = file_first(sp, 0)) == NULL) {
		msgq(sp, M_ERR, "No previous files to rewind.");
		return (1);
	}

	/* Historic practice -- rewind! doesn't do autowrite. */
	if (!F_ISSET(cmdp, E_FORCE))
		MODIFY_CHECK(sp, ep, 0);

	/* Turn off the edited bit. */
	for (tfrp = sp->frefhdr.next;
	    tfrp != (FREF *)&sp->frefhdr; tfrp = tfrp->next)
		F_CLR(tfrp, FR_EDITED);

	if ((sp->n_ep = file_init(sp, NULL, frp, NULL)) == NULL)
		return (1);
	sp->n_frp = frp;
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
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
	FREF *frp;
	int cnt, col, len, sep;

	col = len = sep = 0;
	for (cnt = 1, frp = sp->frefhdr.next;
	    frp != (FREF *)&sp->frefhdr; frp = frp->next) {
		/*
		 * Ignore files that aren't in the "argument" list unless
		 * they are the one we're currently editing.  I'm not sure
		 * this is right, but the historic vi behavior of not
		 * showing the current file if it was the result of a ":e"
		 * command seems wrong.
		 */
		if (F_ISSET(frp, FR_IGNORE) && frp != sp->frp)
			continue;
		col += len = frp->nlen + sep + (frp == sp->frp ? 2 : 0);
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)fprintf(sp->stdfp, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)fprintf(sp->stdfp, " ");
		}
		if (frp == sp->frp)
			(void)fprintf(sp->stdfp, "[%s]", frp->fname);
		else
			(void)fprintf(sp->stdfp, "%s", frp->fname);
		++cnt;
	}
	if (cnt == 1)
		(void)fprintf(sp->stdfp, "No files.\n");
	else
		(void)fprintf(sp->stdfp, "\n");
	return (0);
}
