/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.7 1992/04/28 13:27:15 bostic Exp $ (Berkeley) $Date: 1992/04/28 13:27:15 $";
#endif /* not lint */

#include <sys/types.h>
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
	return (ex_cfile(cmdp->argv[0], 1));
}
