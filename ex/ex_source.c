/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.9 1992/12/05 11:08:54 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:54 $";
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
ex_source(cmdp)
	EXCMDARG *cmdp;
{
	return (ex_cfile((char *)cmdp->argv[0], 1));
}
