/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.14 1993/01/11 18:08:30 bostic Exp $ (Berkeley) $Date: 1993/01/11 18:08:30 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

int
ex_delete(cmdp)
	EXCMDARG *cmdp;
{
	/* Yank the lines. */
	if (cut(curf, cmdp->buffer != OOBCB ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Delete the lines. */
	delete(curf, &cmdp->addr1, &cmdp->addr2, 1);

	/*
	 * Deletion sets the cursor to the line after the last line deleted,
	 * or the last line in the file if delete to the end of the file.
	 */
	curf->lno = cmdp->addr2.lno;
	if (curf->lno > file_lline(curf))
		curf->lno = file_lline(curf);
	curf->cno = 0;

	/* Set autoprint. */
	FF_SET(curf, F_AUTOPRINT);

	return (0);
}
