/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_screen.c,v 9.5 1995/01/23 18:32:08 bostic Exp $ (Berkeley) $Date: 1995/01/23 18:32:08 $";
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
	return (svi_split(sp, cmdp->argc ? cmdp->argv : NULL, cmdp->argc));
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
	enum adjust adj;

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
