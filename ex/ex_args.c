/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 8.9 1993/11/22 08:50:01 bostic Exp $ (Berkeley) $Date: 1993/11/22 08:50:01 $";
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
 *
 * !!!
 * The :next command behaved differently from the :rewind command in
 * historic vi.  See nvi/docs/autowrite for details, but the basic
 * idea was that it ignored the force flag if the autowrite flag was
 * set.  This implementation handles them all identically.
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
		for (frp = sp->frefq.tqh_first;
		    frp != NULL; frp = frp->q.tqe_next)
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

	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);
	F_SET(sp, S_FSWITCH);
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

	if (file_init(sp, sp->p_frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);
	F_SET(sp, S_FSWITCH);
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

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	/*
	 * !!!
	 * Historic practice, turn off the edited bit and discard
	 * any name changes.  Start at the beginning of the file,
	 * too.
	 */
	for (tfrp = sp->frefq.tqh_first;
	    tfrp != NULL; tfrp = tfrp->q.tqe_next) {
		F_CLR(tfrp, FR_CHANGEWRITE | FR_CURSORSET | FR_EDITED);
		if (tfrp->cname != NULL) {
			FREE(tfrp->cname, tfrp->clen);
			tfrp->cname = NULL;
		}
	}

	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);
	F_SET(sp, S_FSWITCH);
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
	int cnt, col, iscur, len, nlen, sep;
	char *name;

	col = len = sep = 0;
	for (cnt = 1,
	    frp = sp->frefq.tqh_first; frp != NULL; frp = frp->q.tqe_next) {
		/*
		 * !!!
		 * Ignore files that aren't in the "argument" list unless
		 * they are the one we're currently editing.  I'm not sure
		 * this is right, but the historic vi behavior of not
		 * showing the current file if it was the result of a ":e"
		 * command seems wrong.
		 */
		if (F_ISSET(frp, FR_IGNORE) && frp != sp->frp)
			continue;
		if (frp->name == NULL) {
			name = frp->tname;
			nlen = frp->tlen;
		} else {
			name = frp->name;
			nlen = frp->nlen;
		}
		iscur = frp == sp->frp && frp->cname == NULL;
extra:		col += len = nlen + sep + (iscur ? 2 : 0);
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)ex_printf(EXCOOKIE, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)ex_printf(EXCOOKIE, " ");
		}
		++cnt;

		/*
		 * !!!
		 * Historic practice was to display the original name of the
		 * file if the user had used an file command to chane the file
		 * name.  Confusing, at best.  We show both names: the original
		 * as that's what the user will get in a rewind, and the new
		 * one as that's what the user is actually editing now.
		 */
		if (iscur)
			(void)ex_printf(EXCOOKIE, "[%s]", name);
		else {
			(void)ex_printf(EXCOOKIE, "%s", name);
			if (frp == sp->frp) {
				name = frp->cname;
				nlen = frp->clen;
				iscur = 1;
				goto extra;
			}
		}
	}
	if (cnt == 1)
		(void)ex_printf(EXCOOKIE, "No files.\n");
	else
		(void)ex_printf(EXCOOKIE, "\n");
	return (0);
}
