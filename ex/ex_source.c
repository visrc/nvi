/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.3 1992/04/04 10:02:45 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:45 $";
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
void
ex_source(cmdp)
	CMDARG *cmdp;
{
	exfile(cmdp->argv[0], 1);
}
