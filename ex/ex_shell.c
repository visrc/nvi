/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shell.c,v 5.5 1992/04/19 08:54:00 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:54:00 $";
#endif /* not lint */

#include <sys/param.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

/*
 * ex_shell (:sh[ell])
 *	Invoke the program named in the SHELL environment variable with
 *	the argument -i.
 */
int
ex_shell(cmdp)
	EXCMDARG *cmdp;
{
	int rval;
	char buf[MAXPATHLEN];

	suspend_curses();

	(void)snprintf(buf, sizeof(buf), "%s -i", PVAL(O_SHELL));
	rval = system(buf);

	resume_curses(mode == MODE_EX);
	return (rval);
}
