/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.5 1992/04/19 08:54:06 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:54:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_version -- (:version)
 *	Display the program version.
 */
/* ARGSUSED */
int
ex_version(cmdp)
	EXCMDARG *cmdp;
{
	msg("Version 1.0 (Berkeley): %s", "$Date: 1992/04/19 08:54:06 $");
	return (0);
}
