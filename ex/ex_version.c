/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 8.12 1993/10/04 16:46:42 bostic Exp $ (Berkeley) $Date: 1993/10/04 16:46:42 $";
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
	    "Version 0.75 (The CSRG, U.C. Berkeley.), %s\n", "$Date: 1993/10/04 16:46:42 $");
	return (0);
}
