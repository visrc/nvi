/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_preserve.c,v 8.5 1994/03/08 19:39:33 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:39:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_preserve -- :pre[serve]
 *	Push the file to recovery.
 */
int
ex_preserve(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	if (!F_ISSET(ep, F_RCV_ON)) {
		msgq(sp, M_ERR, "Preservation of this file not possible.");
		return (1);
	}

	/* If recovery not initialized, do so. */
	if (F_ISSET(ep, F_FIRSTMODIFY) && rcv_init(sp, ep))
		return (1);

	/* Force the file to be read in, in case it hasn't yet. */
	if (file_lline(sp, ep, &lno))
		return (1);

	/* Sync to disk. */
	if (rcv_sync(sp, ep))
		return (1);

	/* Preserve the recovery files. */
	F_SET(ep, F_RCV_NORM);

	msgq(sp, M_INFO, "File preserved.");
	return (0);
}
