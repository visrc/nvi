/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.15 1993/02/16 20:10:08 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:08 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

int
ex_delete(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* Yank the lines. */
	if (cut(ep, cmdp->buffer != OOBCB ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Delete the lines. */
	delete(ep, &cmdp->addr1, &cmdp->addr2, 1);

	/*
	 * Deletion sets the cursor to the line after the last line deleted,
	 * or the last line in the file if delete to the end of the file.
	 */
	ep->lno = cmdp->addr2.lno;
	if (ep->lno > file_lline(ep))
		ep->lno = file_lline(ep);
	ep->cno = 0;

	/* Set autoprint. */
	FF_SET(ep, F_AUTOPRINT);

	return (0);
}
