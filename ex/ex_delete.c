/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.16 1993/02/24 12:55:12 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:55:12 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "screen.h"

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
	SCRLNO(ep) = cmdp->addr2.lno;
	if (SCRLNO(ep) > file_lline(ep))
		SCRLNO(ep) = file_lline(ep);
	SCRCNO(ep) = 0;

	/* Set autoprint. */
	FF_SET(ep, F_AUTOPRINT);

	return (0);
}
