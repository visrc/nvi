/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.6 1992/05/07 12:46:35 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:46:35 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_delete(cmdp)
	EXCMDARG *cmdp;
{
	/* Yank the lines. */
	if (cmdp->buffer)
		cutname(cmdp->buffer);
	cut(&cmdp->addr1, &cmdp->addr2);

	/* Delete the lines. */
	delete(&cmdp->addr1, &cmdp->addr2);
	if (cursor.lno > cmdp->addr2.lno)
		cursor.lno -= cmdp->addr2.lno - cmdp->addr1.lno;
	else if (cursor.lno > cmdp->addr1.lno)
		cursor = cmdp->addr1;

	autoprint = 1;
	return (0);
}
