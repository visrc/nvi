/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 8.19 1994/06/27 11:22:09 bostic Exp $ (Berkeley) $Date: 1994/06/27 11:22:09 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_next -- :next [+cmd] [files]
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
	ARGS **argv, **pc;
	FREF *frp;
	char **ap;

	MODIFY_RET(sp, ep, F_ISSET(cmdp, E_FORCE));

	/*
	 * If the first argument is a plus sign, '+', it's an initial
	 * ex command.
	 */
	argv = cmdp->argv;
	if (cmdp->argc && argv[0]->bp[0] == '+') {
		--cmdp->argc;
		pc = argv++;
	} else
		pc = NULL;

	/* Any other arguments are a replacement file list. */
	if (cmdp->argc) {
		/* Free the current list. */
		if (!F_ISSET(sp, S_ARGNOFREE) && sp->argv != NULL) {
			for (ap = sp->argv; *ap != NULL; ++ap)
				free(*ap);
			free(sp->argv);
		}
		F_CLR(sp, S_ARGNOFREE | S_ARGRECOVER);
		sp->cargv = NULL;

		/* Create a new list. */
		CALLOC_RET(sp,
		    sp->argv, char **, cmdp->argc + 1, sizeof(char *));
		for (ap = sp->argv,
		    argv = cmdp->argv; argv[0]->len != 0; ++ap, ++argv)
			if ((*ap =
			    v_strdup(sp, argv[0]->bp, argv[0]->len)) == NULL)
				return (1);
		*ap = NULL;

		/* Switch to the first one. */
		sp->cargv = sp->argv;
		if ((frp = file_add(sp, *sp->cargv)) == NULL)
			return (1);
	} else {
		if (sp->cargv[1] == NULL) {
			msgq(sp, M_ERR, "No more files to edit");
			return (1);
		}
		if ((frp = file_add(sp, *++sp->cargv)) == NULL)
			return (1);
		if (F_ISSET(sp, S_ARGNOFREE))
			F_SET(frp, FR_RECOVER);
	}

	if (file_init(sp, frp, NULL, F_ISSET(cmdp, E_FORCE)))
		return (1);

	/* Push the initial command onto the stack. */
	if (pc != NULL)
		if (IN_EX_MODE(sp))
			(void)term_push(sp, pc[0]->bp, pc[0]->len, 0);
		else if (IN_VI_MODE(sp)) {
			(void)term_push(sp, "\n", 1, 0);
			(void)term_push(sp, pc[0]->bp, pc[0]->len, 0);
			(void)term_push(sp, ":", 1, 0);
			(void)file_lline(sp, sp->ep, &sp->frp->lno);
			F_SET(sp->frp, FR_CURSORSET);
		}

	F_SET(sp, S_FSWITCH);
	return (0);
}

/*
 * ex_rew -- :rew
 *	Re-edit the list of files.
 */
int
ex_rew(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	FREF *frp;

	/*
	 * !!!
	 * Historic practice -- you can rewind to the current file.
	 */
	if (sp->argv == NULL) {
		msgq(sp, M_ERR, "No previous files to rewind");
		return (1);
	}

	MODIFY_RET(sp, ep, F_ISSET(cmdp, E_FORCE));

	/*
	 * !!!
	 * Historic practice, start at the beginning of the file.
	 */
	for (frp = sp->frefq.cqh_first;
	    frp != (FREF *)&sp->frefq; frp = frp->q.cqe_next)
		F_CLR(frp, FR_CURSORSET);
	
	/* Switch to the first one. */
	sp->cargv = sp->argv;
	if ((frp = file_add(sp, *sp->cargv)) == NULL)
		return (1);
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
	int cnt, col, iscur, len, nlen, sep;
	char **ap, *name;

	if (sp->argv == NULL) {
		(void)ex_printf(EXCOOKIE, "No file list to display.\n");
		return (0);
	}
		
	col = len = sep = 0;
	for (cnt = 1, ap = sp->argv; *ap != NULL; ++ap) {
		name = *ap;
		iscur = ap == sp->cargv &&
		    !strcmp(*ap, F_ISSET(sp->frp, FR_TMPFILE) ?
		    TEMPORARY_FILE_STRING : sp->frp->name);
extra:		nlen = strlen(name);
		col += len = nlen + sep + (iscur ? 2 : 0);
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)ex_printf(EXCOOKIE, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)ex_printf(EXCOOKIE, " ");
		}
		++cnt;

		if (iscur)
			(void)ex_printf(EXCOOKIE, "[%s]", name);
		else {
			(void)ex_printf(EXCOOKIE, "%s", name);
			if (ap == sp->cargv) {
				name = sp->frp->name;
				iscur = 1;
				goto extra;
			}
		}
	}
	return (0);
}
