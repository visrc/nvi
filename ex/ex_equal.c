/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 5.12 1993/05/15 21:22:10 bostic Exp $ (Berkeley) $Date: 1993/05/15 21:22:10 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_equal -- :address =
 *	Print out the line number matching the specified address, or the
 *	last line number in the file if no address specified.
 */
int
ex_equal(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	if (cmdp->addrcnt)
		(void)fprintf(sp->stdfp, "%ld", cmdp->addr1.lno);
	else {
		if (file_lline(sp, ep, &lno))
			return (1);
		(void)fprintf(sp->stdfp, "%ld", lno);
	}
	return (0);
}
