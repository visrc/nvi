/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_screen.c,v 9.7 1995/01/30 15:11:01 bostic Exp $ (Berkeley) $Date: 1995/01/30 15:11:01 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
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

/*
 * ex_split --	:s[plit] [file ...]
 *	Split the screen, optionally setting the file list.
 */
int
ex_split(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	ARGS **argv;
	SCR *top, *bot, *new;
	int argc;
	char **ap;
	
	if (svi_split(sp, &top, &bot))
		return (1);
	new = sp == top ? bot : top;

	/*
	 * If no files specified, link to the current file, keeping screen
	 * and cursor the same, and copying the file state flags.  (The
	 * split routine filled in the screen map.)   Otherwise, build a
	 * new file list, and start on the first file.
	 */
	if (cmdp->argc == 0) {
		if ((new->frp = file_add(new, sp->frp->name)) == NULL)
			goto err;
		new->ep = sp->ep;
		++sp->ep->refcnt;

		new->frp->flags = sp->frp->flags;
		new->lno = sp->lno;
		new->cno = sp->cno;
	} else {
		argc = cmdp->argc;
		argv = cmdp->argv;
		CALLOC(sp, new->argv, char **, argc + 1, sizeof(char *));
		if (new->argv == NULL)
			goto err;
		for (ap = new->argv, argv; argv[0]->len != 0; ++ap, ++argv)
			if ((*ap =
			    v_strdup(sp, argv[0]->bp, argv[0]->len)) == NULL)
				goto err;
		*ap = NULL;

		/* Switch to the first file in the list. */
		new->cargv = new->argv;
		if ((new->frp = file_add(new, *new->cargv)) == NULL)
			goto err;
		if (file_init(new, new->frp, NULL, 0)) {
err:			if (sp == top)
				(void)svi_join(new, sp, NULL, NULL);
			else
				(void)svi_join(new, NULL, sp, NULL);
			(void)screen_end(new);
			return (1);
		}
	}

	/* Everything's initialized, put the screen on the displayed queue.*/
	SIGBLOCK(sp->gp);
	if (sp == bot) {
		/* Split up, link in before the parent. */
		CIRCLEQ_INSERT_BEFORE(&sp->gp->dq, sp, new, q);
	} else {
		/* Split down, link in after the parent. */
		CIRCLEQ_INSERT_AFTER(&sp->gp->dq, sp, new, q);
	}
	SIGUNBLOCK(sp->gp);

	/* Redraw the child from scratch. */
	F_SET(new, S_SCR_REFORMAT);

	/* Save the parent screen's cursor information. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	/* Draw the status line for both screens. */
	(void)msg_status(sp, sp->lno, 0);
	(void)msg_status(new, new->lno, 0);

	/* Switch screens. */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);
	return (0);
}

/*
 * ex_bg --	:bg
 *	Hide the screen.
 */
int
ex_bg(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (svi_bg(sp));
}

/*
 * ex_fg --	:fg [file]
 *	Show the screen.
 */
int
ex_fg(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (svi_fg(sp, cmdp->argc ? cmdp->argv[0]->bp : NULL));
}

/*
 * ex_resize --	:resize [+-]rows
 *	Change the screen size.
 */
int
ex_resize(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	adj_t adj;

	if (!F_ISSET(cmdp, E_COUNT)) {
		ex_message(sp, cmdp->cmd, EXM_USAGE);
		return (1);
	}
	if (F_ISSET(cmdp, E_COUNT_NEG))
		adj = A_DECREASE;
	else if (F_ISSET(cmdp, E_COUNT_POS))
		adj = A_INCREASE;
	else
		adj = A_SET;
	return (svi_rabs(sp, cmdp->count, adj));
}

/*
 * ex_sdisplay --
 *	Display the list of screens.
 */
int
ex_sdisplay(sp)
	SCR *sp;
{
	SCR *tsp;
	int cnt, col, len, sep;

	if ((tsp = sp->gp->hq.cqh_first) == (void *)&sp->gp->hq) {
		msgq(sp, M_INFO, "261|No background screens to display");
		return (0);
	}

	col = len = sep = 0;
	for (cnt = 1; tsp != (void *)&sp->gp->hq && !INTERRUPTED(sp);
	    tsp = tsp->q.cqe_next) {
		col += len = strlen(tsp->frp->name) + sep;
		if (col >= sp->cols - 1) {
			col = len;
			sep = 0;
			(void)ex_printf(EXCOOKIE, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)ex_printf(EXCOOKIE, " ");
		}
		(void)ex_printf(EXCOOKIE, "%s", tsp->frp->name);
		++cnt;
	}
	if (!INTERRUPTED(sp))
		(void)ex_printf(EXCOOKIE, "\n");

	F_SET(sp, S_SCR_EXWROTE);
	return (0);
}
