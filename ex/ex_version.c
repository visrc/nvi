/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.14 1993/05/06 00:34:58 bostic Exp $ (Berkeley) $Date: 1993/05/06 00:34:58 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_version -- :version
 *	Display the program version.
 */
int
ex_version(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	(void)fprintf(sp->stdfp,
	    "Version 0.2 (The CSRG, U.C. Berkeley.): %s", "$Date: 1993/05/06 00:34:58 $");
	return (0);
}
