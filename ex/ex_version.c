/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 5.1 1992/03/15 18:32:54 bostic Exp $ (Berkeley) $Date: 1992/03/15 18:32:54 $";
#endif /* not lint */

#include "vi.h"
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
	msg(sccsid);
}
