/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 8.3 1993/11/04 16:16:54 bostic Exp $ (Berkeley) $Date: 1993/11/04 16:16:54 $";
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
	if (cut(sp, ep,
	    F_ISSET(cmdp, E_BUFFER) ? cmdp->buffer : DEFCB,
	    &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Delete the lines. */
	if (delete(sp, ep, &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Set the cursor to the line after the last line deleted. */
	sp->lno = cmdp->addr1.lno;

	/* Or the last line in the file if deleted to the end of the file. */
	if (file_lline(sp, ep, &lno))
		return (1);
	if (sp->lno > lno)
		sp->lno = lno;

	/* Set autoprint. */
	F_SET(sp, S_AUTOPRINT);

	return (0);
}
