/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.10 1993/02/16 20:10:32 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:32 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_version -- :version
 *	Display the program version.
 */
int
ex_version(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	(void)fprintf(ep->stdfp,
	    "Version 0.1 (The CSRG, U.C. Berkeley.): %s", "$Date: 1993/02/16 20:10:32 $");
	return (0);
}
