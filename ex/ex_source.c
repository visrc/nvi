/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.11 1993/03/25 15:00:07 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:07 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_source -- :source file
 *	Execute ex commands from a file.
 */
int
ex_source(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (ex_cfile(sp, ep, (char *)cmdp->argv[0], 1));
}
