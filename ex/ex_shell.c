/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 5.21 1993/02/25 19:41:26 bostic Exp $ (Berkeley) $Date: 1993/02/25 19:41:26 $";
#endif /* not lint */

#include <sys/param.h>

#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable
 *	with the argument -i.
 */
int
ex_shell(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	extern struct termios original_termios;
	struct termios t;
	int rval;
	char buf[MAXPATHLEN];

	MODIFY_WARN(ep);

	/* Save ex/vi terminal settings, and restore the original ones. */
	(void)tcgetattr(STDIN_FILENO, &t);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &original_termios);

	/* Start with a new line. */
	(void)write(STDOUT_FILENO, "\n", 1);

	(void)snprintf(buf, sizeof(buf), "%s -i", PVAL(O_SHELL));
	rval = esystem(ep, PVAL(O_SHELL), (u_char *)buf);

	/* Restore ex/vi terminal settings. */
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

	/* Repaint the screen. */
	SF_SET(ep, S_REFRESH);
	return (rval);
}
