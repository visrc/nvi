/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.9 1993/01/24 20:05:48 bostic Exp $ (Berkeley) $Date: 1993/01/24 20:05:48 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_version -- :version
 *	Display the program version.
 */
/* ARGSUSED */
int
ex_version(cmdp)
	EXCMDARG *cmdp;
{
	msg("Version 0.1 (The CSRG, U.C. Berkeley.): %s", "$Date: 1993/01/24 20:05:48 $");
	return (0);
}
