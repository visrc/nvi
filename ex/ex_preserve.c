/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_preserve.c,v 9.3 1995/01/11 16:15:42 bostic Exp $ (Berkeley) $Date: 1995/01/11 16:15:42 $";
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
ex_preserve(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	recno_t lno;

	NEEDFILE(sp, cmdp->cmd);

	if (!F_ISSET(sp->ep, F_RCV_ON)) {
		msgq(sp, M_ERR, "147|Preservation of this file not possible");
		return (1);
	}

	/* If recovery not initialized, do so. */
	if (F_ISSET(sp->ep, F_FIRSTMODIFY) && rcv_init(sp))
		return (1);

	/* Force the file to be read in, in case it hasn't yet. */
	if (file_lline(sp, &lno))
		return (1);

	/* Sync to disk. */
	if (rcv_sync(sp, RCV_SNAPSHOT))
		return (1);

	msgq(sp, M_INFO, "148|File preserved");
	return (0);
}

/*
 * ex_recover -- :rec[over][!] file
 *
 * Recover the file.
 */
int
ex_recover(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	ARGS *ap;
	FREF *frp;

	ap = cmdp->argv[0];

	/* Set the alternate file name. */
	set_alt_name(sp, ap->bp);

	/*
	 * Check for modifications.  Autowrite did not historically
	 * affect :recover.
	 */
	if (file_m2(sp, F_ISSET(cmdp, E_FORCE)))
		return (1);

	/* Get a file structure for the file. */
	if ((frp = file_add(sp, ap->bp)) == NULL)
		return (1);

	/* Set the recover bit. */
	F_SET(frp, FR_RECOVER);

	/* Switch files. */
	if (file_init(sp, frp, NULL,
	    FS_SETALT | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0)))
		return (1);
	(void)msg_status(sp, sp->lno, 0);
	return (0);
}
