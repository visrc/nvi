/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.5 1992/04/15 09:14:07 bostic Exp $ (Berkeley) $Date: 1992/04/15 09:14:07 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_source -- (:source file)
 *	Execute ex commands from a file.
 */
int
ex_source(cmdp)
	CMDARG *cmdp;
{
	return (ex_cfile(cmdp->argv[0], 1));
}
