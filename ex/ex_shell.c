/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 8.2 1993/09/28 15:21:44 bostic Exp $ (Berkeley) $Date: 1993/09/28 15:21:44 $";
#endif /* not lint */

#include <sys/param.h>

#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable
 *	with the argument -i.
 */
int
ex_shell(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	struct termios t;
	int rval;
	char buf[MAXPATHLEN];

#ifdef SHELL_PROCESS
	if (ex_run_shell(sp))
		return (1);
#else
	/* Save ex/vi terminal settings, and restore the original ones. */
	(void)tcgetattr(STDIN_FILENO, &t);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &sp->gp->original_termios);

	/* Start with a new line. */
	(void)write(STDOUT_FILENO, "\n", 1);

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));
	rval = esystem(sp, O_STR(sp, O_SHELL), buf);

	/* Restore ex/vi terminal settings. */
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	/* Repaint the screen. */
	F_SET(sp, S_REFRESH);
#endif
	return (rval);
}
