/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 5.9 1993/02/16 20:10:10 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:10 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_equal -- :address =
 *	Print out the line number matching the specified address, or the
 *	last line number in the file if no address specified.
 */
int
ex_equal(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	(void)fprintf(ep->stdfp,
	    "%ld", cmdp->addrcnt ? cmdp->addr1.lno : file_lline(ep));
	return (0);
}
