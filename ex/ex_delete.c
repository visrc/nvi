/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 8.10 1994/07/20 16:28:03 bostic Exp $ (Berkeley) $Date: 1994/07/20 16:28:03 $";
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
ex_delete(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	/*
	 * !!!
	 * Historically, delete permitted an address of 0, i.e.
	 * an empty file.
	 */
	if (cmdp->addr1.lno == 0)
		return (0);

	/* Yank the lines. */
	if (cut(sp, ep,
	    F_ISSET(cmdp, E_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE | CUT_NBUFFER))
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
	return (0);
}
