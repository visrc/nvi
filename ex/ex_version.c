/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 8.19 1993/11/22 19:49:41 bostic Exp $ (Berkeley) $Date: 1993/11/22 19:49:41 $";
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
	static time_t then = 754015782;

	(void)ex_printf(EXCOOKIE,
"Version 0.84, %sThe CSRG, University of California, Berkeley.\n",
	    ctime(&then));
	return (0);
}
