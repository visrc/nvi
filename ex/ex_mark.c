/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.1 1992/04/02 11:21:04 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:04 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_mark(cmdp)
	CMDARG *cmdp;
{
	char *mark;

	if (cmdp->argc == 0) {
		msg("No mark name specified.");
		return;
	}

	/*
	 * Validate the name of the mark.  Valid mark names are lowercase
	 * ascii characters.
	 */
	mark = cmdp->argv[0];
	if (!isascii(*mark) || !islower(*mark) || mark[1]) {
		msg("Invalid mark name: %s", mark);
		return;
	}
	mark[*mark - 'a'] = cmdp->addr2;
}
