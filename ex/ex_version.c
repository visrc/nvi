/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.8 1992/12/05 11:09:00 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:09:00 $";
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
	msg("Version 1.0 (The CSRG, U.C. Berkeley.): %s", "$Date: 1992/12/05 11:09:00 $");
	return (0);
}
