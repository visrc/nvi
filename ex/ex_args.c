/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_args.c,v 5.14 1992/05/04 11:51:35 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:51:35 $";
#endif /* not lint */

#include <sys/types.h>
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "exf.h"
#include "options.h"
#include "tty.h"
#include "extern.h"

/*
 * ex_next -- :next [files]
 *	Edit the next file.
 */
int
ex_next(cmdp)
	EXCMDARG *cmdp;
{
	EXF *ep;

	DEFMODSYNC;

	/*
	 * If a file list specified or there are more files to edit, the
	 * next command will succeed.  Toss the last file.
	 */
	if (cmdp->argc || file_next(curf) != NULL) {
		if (file_stop(curf, 0))
			return (1);
	} else {
		msg("No more files to edit.");
		return (1);
	}

	/* Take any file list specified. */
	if (cmdp->argc)
		file_set(cmdp->argc, cmdp->argv);

	return (file_start(file_next(curf)));
}

/*
 * ex_prev -- :prev
 *	Edit the previous file.
 */
int
ex_prev(cmdp)
	EXCMDARG *cmdp;
{
	EXF *ep;

	if ((ep = file_prev(curf)) == NULL) {
		msg("No previous files to edit.");
		return (1);
	}

	DEFMODSYNC;
	if (file_stop(curf, 0))
		return (1);
	return (file_start(ep));
}

/*
 * ex_rew -- :rew
 *	Edit the first file.
 */
int
ex_rew(cmdp)
	EXCMDARG *cmdp;
{
	if (file_prev(curf) == NULL) {
		msg("No previous files to rewind.");
		return (1);
	}

	DEFMODSYNC;

	if (file_stop(curf, 0))
		return (1);
	return (file_start(file_first()));
}

/*
 * ex_args -- :args
 *	Display the list of files.
 */
int
ex_args(cmdp)
	EXCMDARG *cmdp;
{
	register EXF *ep;
	register int cnt, col, sep;
	int len;

	ep = file_first();
	if (ep->name == NULL) {
		msg("No file names.");
		return (1);
	}

	EX_PRSTART(1);
	col = len = sep = 0;
	for (cnt = 1; ep; ++cnt) {
		col += len = strlen(ep->name) + sep + (curf == ep ? 2 : 0);
		if (col >= COLS - 1) {
			col = len;
			sep = 0;
			EX_PRNEWLINE;
		} else if (cnt != 1) {
			sep = 1;
			(void)putchar(' ');
		}
		if (curf == ep)
			(void)printf("[%s]", ep->name);
		else
			(void)printf("%s", ep->name);
		ep = file_next(ep);
	}
	EX_PRTRAIL;
	return (0);
}
