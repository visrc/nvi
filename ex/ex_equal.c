/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 5.1 1992/04/02 11:20:53 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:20:53 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_equal (:address =)
 *	Print out the line number matching the specified address, or the
 *	last line number in the file if no address specified.
 */
void
ex_equal(cmdp)
	CMDARG *cmdp;
{
	msg("%ld", markline(cmdp->addrcnt ? cmdp->addr1 : MARK_LAST));
}
