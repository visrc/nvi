/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.10 1992/10/17 16:09:59 bostic Exp $ (Berkeley) $Date: 1992/10/17 16:09:59 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_delete(cmdp)
	EXCMDARG *cmdp;
{
	/* Yank the lines. */
	if (cut(cmdp->buffer != OOBCB ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Delete the lines. */
	delete(&cmdp->addr1, &cmdp->addr2);

	/* Adjust the cursor. */
	if (curf->lno > cmdp->addr2.lno)
		curf->lno -= cmdp->addr2.lno - cmdp->addr1.lno;
	else if (curf->lno > cmdp->addr1.lno) {
		curf->lno = cmdp->addr1.lno;
		curf->cno = cmdp->addr1.cno;
	}

	autoprint = 1;
	return (0);
}
