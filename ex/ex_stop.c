/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_stop.c,v 5.4 1993/02/28 14:00:43 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:43 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"

/*
 * ex_stop -- :stop
 *	Suspend execution.
 */
int
ex_stop(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (!(cmdp->flags & E_FORCE)) {
		AUTOWRITE(ep);
	}

	return (ex_suspend(ep));
}

/*
 * ex_suspend --
 *	Suspend execution.
 */
int
ex_suspend(ep)
	EXF *ep;
{
	extern struct termios original_termios;
	struct termios t;

	/* Save ex/vi terminal settings, and restore the original ones. */
	(void)tcgetattr(STDIN_FILENO, &t);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &original_termios);

	if (kill(0, SIGTSTP)) {
		ep->msg(ep, M_ERROR, "Error: SIGTSTP: %s", strerror(errno));
		return (1);
	}

	/* Restore ex/vi terminal settings. */
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	/* Repaint the screen. */
	SF_SET(ep, S_REFRESH);
	return (0);
}
