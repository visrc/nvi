/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.10 1993/02/16 20:10:25 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:25 $";
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
ex_source(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (ex_cfile(ep, (char *)cmdp->argv[0], 1));
}
