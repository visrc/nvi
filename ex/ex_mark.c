/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 5.5 1992/05/07 12:46:54 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:46:54 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_mark(cmdp)
	EXCMDARG *cmdp;
{
	char *mc;

	if (cmdp->argc == 0) {
		msg("No mark name specified.");
		return (1);;
	}

	/*
	 * Validate the name of the mark.  Valid mark names are lowercase
	 * ascii characters.
	 */
	mc = cmdp->argv[0];
	if (!isascii(*mc) || !islower(*mc) || mc[1]) {
		msg("Invalid mark name: %s", mc);
		return (1);;
	}
	mark[*mc - 'a'] = cmdp->addr2;
	return (0);
}
