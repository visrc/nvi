/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 5.14 1992/12/05 11:08:52 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:52 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable with
 *	the argument -i.
 */
int
ex_shell(cmdp)
	EXCMDARG *cmdp;
{
	struct termios t;
	int rval;
	char buf[MAXPATHLEN];

	MODIFY_WARN(curf);

	(void)tcgetattr(STDIN_FILENO, &t);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &origtermio);
	(void)snprintf(buf, sizeof(buf), "%s -i", PVAL(O_SHELL));
	rval = esystem(PVAL(O_SHELL), (u_char *)buf);
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
	return (rval);
}
