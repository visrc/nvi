/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_yank.c,v 5.1 1992/05/15 11:10:29 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:10:29 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_yank -- :[line [,line]] ya[nk] [buffer] [count]
 *	Yank the lines into a buffer.
 */
int
ex_yank(cmdp)
	EXCMDARG *cmdp;
{
	/* Yank the lines. */
	if (cmdp->buffer)
		cutname(cmdp->buffer);
	cut(&cmdp->addr1, &cmdp->addr2);

	autoprint = 1;
	return (0);
}
