/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.18 1993/05/16 12:41:05 bostic Exp $ (Berkeley) $Date: 1993/05/16 12:41:05 $";
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
	    "Version 0.3 (The CSRG, U.C. Berkeley.): %s", "$Date: 1993/05/16 12:41:05 $");
	return (0);
}
