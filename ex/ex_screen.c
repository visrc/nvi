/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_screen.c,v 10.2 1995/05/05 18:51:38 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:51:38 $";
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

#include "common.h"
#include "vi.h"

/*
 * ex_bg --	:bg
 *	Hide the screen.
 *
 * PUBLIC: int ex_bg __P((SCR *, EXCMD *));
 */
int
ex_bg(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (vs_bg(sp));
}

/*
 * ex_fg --	:fg [file]
 *	Show the screen.
 *
 * PUBLIC: int ex_fg __P((SCR *, EXCMD *));
 */
int
ex_fg(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (vs_fg(sp, cmdp->argc ? cmdp->argv[0]->bp : NULL));
}

/*
 * ex_resize --	:resize [+-]rows
 *	Change the screen size.
 *
 * PUBLIC: int ex_resize __P((SCR *, EXCMD *));
 */
int
ex_resize(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	adj_t adj;

	switch (FL_ISSET(cmdp->iflags,
	    E_C_COUNT | E_C_COUNT_NEG | E_C_COUNT_POS)) {
	case E_C_COUNT:
		adj = A_SET;
		break;
	case E_C_COUNT | E_C_COUNT_NEG:
		adj = A_DECREASE;
		break;
	case E_C_COUNT | E_C_COUNT_POS:
		adj = A_INCREASE;
		break;
	default:
		ex_message(sp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}
	return (vs_resize(sp, cmdp->count, adj));
}

/*
 * ex_sdisplay --
 *	Display the list of screens.
 *
 * PUBLIC: int ex_sdisplay __P((SCR *));
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
			(void)ex_puts(sp, "\n");
		} else if (cnt != 1) {
			sep = 1;
			(void)ex_puts(sp, " ");
		}
		(void)ex_puts(sp, tsp->frp->name);
		++cnt;
	}
	if (!INTERRUPTED(sp))
		(void)ex_puts(sp, "\n");

	F_SET(sp, S_EX_WROTE);
	return (0);
}
