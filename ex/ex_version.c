/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.2 1992/03/30 11:30:24 bostic Exp $ (Berkeley) $Date: 1992/03/30 11:30:24 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_version -- (:version)
 *	Display the program version.
 */
/* ARGSUSED */
void
ex_version(cmdp)
	CMDARG *cmdp;
{
	msg("Version 1.0 (Berkeley): %s", "$Date: 1992/03/30 11:30:24 $");
}
