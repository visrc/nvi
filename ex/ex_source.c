/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.8 1992/10/10 13:58:00 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:58:00 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

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
