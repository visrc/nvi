/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 9.9 1995/02/09 18:21:01 bostic Exp $ (Berkeley) $Date: 1995/02/09 18:21:01 $";
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
#include "../svi/svi_screen.h"

static int ex_N_next __P((SCR *, EXCMDARG *));

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
ex_next(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	ARGS **argv;
	FREF *frp;
	int noargs;
	char **ap;

	/* Check for file to move to. */
	if (cmdp->argc == 0 && (sp->cargv == NULL || sp->cargv[1] == NULL)) {
		msgq(sp, M_ERR, "120|No more files to edit");
		return (1);
	}

	if (F_ISSET(cmdp, E_NEWSCREEN)) {
		/* By default, edit the next file in the old argument list. */
		if (cmdp->argc == 0) {
			if (argv_exp0(sp,
			    cmdp, sp->cargv[1], strlen(sp->cargv[1])))
				return (1);
			return (ex_edit(sp, cmdp));
		}
		return (ex_N_next(sp, cmdp));
	}

	/* Check modification. */
	if (file_m1(sp, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
		return (1);

	/* Any arguments are a replacement file list. */
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

		/* Switch to the first file. */
		sp->cargv = sp->argv;
		if ((frp = file_add(sp, *sp->cargv)) == NULL)
			return (1);
		noargs = 0;
	} else {
		if ((frp = file_add(sp, sp->cargv[1])) == NULL)
			return (1);
		if (F_ISSET(sp, S_ARGRECOVER))
			F_SET(frp, FR_RECOVER);
		noargs = 1;
	}

	if (file_init(sp, frp, NULL,
	    FS_SETALT | FS_WELCOME | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0)))
		return (1);
	if (noargs)
		++sp->cargv;
	return (0);
}

/*
 * ex_N_next --
 *	New screen version of ex_next.
 */
static int
ex_N_next(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	SCR *bot, *new, *top;
	ARGS **argv;
	FREF *frp;
	char **ap, **s_argv;

	/* Any arguments are a replacement file list. */
	sp->cargv = NULL;
	CALLOC_RET(sp, s_argv, char **, cmdp->argc + 1, sizeof(char *));
	for (ap = s_argv, argv = cmdp->argv; argv[0]->len != 0; ++ap, ++argv)
		if ((*ap = v_strdup(sp, argv[0]->bp, argv[0]->len)) == NULL)
			return (1);
	*ap = NULL;

	/* Get a new screen. */
	if (svi_split(sp, &top, &bot))
		return (1);
	new = sp == top ? bot : top;

	/* Switch to the first file. */
	new->cargv = new->argv = s_argv;
	if ((frp = file_add(new, *new->cargv)) == NULL ||
	    file_init(new, frp, NULL,
	    FS_WELCOME | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0))) {
		if (sp == top)
			(void)svi_join(new, sp, NULL, NULL);
		else
			(void)svi_join(new, NULL, sp, NULL);
		(void)screen_end(new);
		return (1);
	}

	/* Add the new screen to the queue. */
	SIGBLOCK(sp->gp);
	if (sp == bot) {
		/* Split up, link in before the parent. */
		CIRCLEQ_INSERT_BEFORE(&sp->gp->dq, sp, new, q);
	} else {
		/* Split down, link in after the parent. */
		CIRCLEQ_INSERT_AFTER(&sp->gp->dq, sp, new, q);
	}
	SIGUNBLOCK(sp->gp);

	/* Set up the switch. */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);

	return (0);
}

/*
 * ex_prev -- :prev
 *	Edit the previous file.
 */
int
ex_prev(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	FREF *frp;

	if (file_m1(sp, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
		return (1);

	if (sp->cargv == sp->argv) {
		msgq(sp, M_ERR, "121|No previous files to edit");
		return (1);
	}
	if ((frp = file_add(sp, sp->cargv[-1])) == NULL)
		return (1);

	if (file_init(sp, frp, NULL,
	    FS_SETALT | FS_WELCOME | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0)))
		return (1);
	--sp->cargv;

	return (0);
}

/*
 * ex_rew -- :rew
 *	Re-edit the list of files.
 *
 * !!!
 * Historic practice was that all files would start editing at the beginning
 * of the file.  We don't get this right because we may have multiple screens
 * and we can't clear the FR_CURSORSET bit for a single screen.  I don't see
 * anyone noticing, but if they do, we'll have to put information into the SCR
 * structure so we can keep track of it.
 */
int
ex_rew(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	FREF *frp;

	/*
	 * !!!
	 * Historic practice -- you can rewind to the current file.
	 */
	if (sp->argv == NULL) {
		msgq(sp, M_ERR, "122|No previous files to rewind");
		return (1);
	}

	if (file_m1(sp, F_ISSET(cmdp, E_FORCE), FS_ALL | FS_POSSIBLE))
		return (1);

	/* Switch to the first one. */
	sp->cargv = sp->argv;
	if ((frp = file_add(sp, *sp->cargv)) == NULL)
		return (1);
	if (file_init(sp, frp, NULL,
	    FS_SETALT | FS_WELCOME | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0)))
		return (1);
	return (0);
}

/*
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	int cnt, col, len, sep;
	char **ap;

	if (sp->argv == NULL) {
		(void)msgq(sp, M_ERR, "263|No file list to display");
		return (0);
	}

	col = len = sep = 0;
	for (cnt = 1, ap = sp->argv; *ap != NULL; ++ap) {
		col += len = strlen(*ap) + sep + (ap == sp->cargv ? 2 : 0);
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)ex_printf(EXCOOKIE, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)ex_printf(EXCOOKIE, " ");
		}
		++cnt;

		if (ap == sp->cargv)
			(void)ex_printf(EXCOOKIE, "[%s]", *ap);
		else
			(void)ex_printf(EXCOOKIE, "%s", *ap);
		if (INTERRUPTED(sp))
			break;
	}
	if (!INTERRUPTED(sp))
		(void)ex_printf(EXCOOKIE, "\n");
	F_SET(sp, S_SCR_EXWROTE);

	return (0);
}
