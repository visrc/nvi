/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 5.6 1992/04/28 13:44:03 bostic Exp $ (Berkeley) $Date: 1992/04/28 13:44:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_put(cmdp)
	EXCMDARG *cmdp;
{
	char *extra;

	extra = cmdp->argv[0];
	/* choose your cut buffer */
	if (*extra == '"')
	{
		extra++;
	}
	if (*extra)
	{
		cutname(*extra);
	}

	/* paste it */
	ChangeText
	{
		cursor = paste(cmdp->addr1, 1, 0);
	}

	autoprint = 1;
	return (0);
}
