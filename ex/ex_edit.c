/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 9.2 1994/12/16 11:08:54 bostic Exp $ (Berkeley) $Date: 1994/12/16 11:08:54 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *		:vi[sual][!] [+cmd] [file]
 *
 * Edit a file; if none specified, re-edit the current file.  The second
 * form of the command can only be executed while in vi mode.  See the
 * hack in ex.c:ex_cmd().
 *
 * !!!
 * Historic vi didn't permit the '+' command form without specifying
 * a file name as well.
 */
int
ex_edit(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	ARGS *ap;
	FREF *frp;
	int setalt;

	frp = sp->frp;
	switch (cmdp->argc) {
	case 0:
		/*
		 * If the name has been changed, we edit that file, not the
		 * original name.  If the user was editing a temporary file
		 * (or wasn't editing any file), create another one.  The
		 * reason for not reusing temporary files is that there is
		 * special exit processing of them, and reuse is tricky.
		 */
		if (sp->ep == NULL || F_ISSET(frp, FR_TMPFILE)) {
			if ((frp = file_add(sp, NULL)) == NULL)
				return (1);
		} else if ((frp = file_add(sp, frp->name)) == NULL)
			return (1);
		setalt = 0;
		break;
	case 1:
		ap = cmdp->argv[0];
		if ((frp = file_add(sp, ap->bp)) == NULL)
			return (1);
		setalt = 1;
		set_alt_name(sp, ap->bp);
		break;
	default:
		abort();
	}

	/*
	 * Check for modifications.
	 *
	 * !!!
	 * Contrary to POSIX 1003.2-1992, autowrite did not affect :edit.
	 */
	if (file_m2(sp, F_ISSET(cmdp, E_FORCE)))
		return (1);

	/* Switch files. */
	if (file_init(sp, frp, NULL,
	    (setalt ? FS_SETALT : 0) | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0)))
		return (1);
	(void)msg_status(sp, sp->lno, 0);
	return (0);
}
