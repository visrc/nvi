/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 8.17 1993/11/21 16:19:36 bostic Exp $ (Berkeley) $Date: 1993/11/21 16:19:36 $";
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
	(void)ex_printf(EXCOOKIE,
"Vers. 0.81, Nov. 6, 1993, The CSRG, University of California, Berkeley.\n");
	return (0);
}
