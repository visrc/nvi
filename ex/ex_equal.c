/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 5.4 1992/04/19 08:53:47 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:53:47 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_equal (:address =)
 *	Print out the line number matching the specified address, or the
 *	last line number in the file if no address specified.
 */
int
ex_equal(cmdp)
	EXCMDARG *cmdp;
{
	msg("%ld", markline(cmdp->addrcnt ? cmdp->addr1 : MARK_LAST));
	return (0);
}
