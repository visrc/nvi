/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 5.7 1993/04/12 14:37:55 bostic Exp $ (Berkeley) $Date: 1993/04/12 14:37:55 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_stop -- :stop
 *	Suspend execution.
 */
int
ex_stop(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (!(cmdp->flags & E_FORCE))
		AUTOWRITE(sp, ep);

	return (ex_suspend(sp));
}

/*
 * ex_suspend --
 *	Suspend execution.
 */
int
ex_suspend(sp)
	SCR *sp;
{
	struct termios t;

	/* Save ex/vi terminal settings, and restore the original ones. */
	(void)tcgetattr(STDIN_FILENO, &t);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &sp->gp->original_termios);

	if (kill(0, SIGTSTP)) {
		msgq(sp, M_ERR, "Error: SIGTSTP: %s", strerror(errno));
		return (1);
	}

	/* Restore ex/vi terminal settings. */
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	/* Repaint the screen. */
	F_SET(sp, S_REFRESH);
	return (0);
}
