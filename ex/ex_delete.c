/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 8.1 1993/06/09 22:23:46 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:23:46 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

int
ex_delete(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	/* Yank the lines. */
	if (cut(sp, ep, cmdp->buffer != OOBCB ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Delete the lines. */
	delete(sp, ep, &cmdp->addr1, &cmdp->addr2, 1);

	/*
	 * Deletion sets the cursor to the line after the last line deleted,
	 * or the last line in the file if delete to the end of the file.
	 */
	sp->lno = cmdp->addr2.lno;
	if (file_lline(sp, ep, &lno))
		return (1);
	if (sp->lno > lno);
		sp->lno = lno;

	/* Set autoprint. */
	F_SET(sp, S_AUTOPRINT);

	return (0);
}
