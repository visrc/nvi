/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_yank.c,v 5.2 1992/05/21 12:57:02 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:57:02 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "cut.h"
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
	return (cut(cmdp->buffer != OOBCB ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1));
}
