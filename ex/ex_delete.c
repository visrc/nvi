/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 9.1 1994/11/09 18:40:37 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:40:37 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_delete: [line [,line]] d[elete] [buffer] [count] [flags]
 *
 *	Delete lines from the file.
 */
int
ex_delete(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	recno_t lno;

	NEEDFILE(sp, cmdp->cmd);

	/*
	 * !!!
	 * Historically, lines deleted in ex were not placed in the numeric
	 * buffers.  We follow historic practice so that we don't overwrite
	 * vi buffers accidentally.
	 */
	if (cut(sp,
	    F_ISSET(cmdp, E_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE))
		return (1);

	/* Delete the lines. */
	if (delete(sp, &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Set the cursor to the line after the last line deleted. */
	sp->lno = cmdp->addr1.lno;

	/* Or the last line in the file if deleted to the end of the file. */
	if (file_lline(sp, &lno))
		return (1);
	if (sp->lno > lno)
		sp->lno = lno;
	return (0);
}
