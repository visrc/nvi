/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 9.7 1995/02/09 12:13:01 bostic Exp $ (Berkeley) $Date: 1995/02/09 12:13:01 $";
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
#include "../svi/svi_screen.h"

static int ex_N_edit __P((SCR *, EXCMDARG *, FREF *, int));

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *		:ex[!] [+cmd] [file]
 *		:vi[sual][!] [+cmd] [file]
 *
 * Edit a file; if none specified, re-edit the current file.  The third
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
	FREF *frp;
	int attach, setalt;

	/*
	 * Check for modifications.
	 *
	 * !!!
	 * Contrary to POSIX 1003.2-1992, autowrite did not affect :edit.
	 */
	if (file_m2(sp, F_ISSET(cmdp, E_FORCE)))
		return (1);

	switch (cmdp->argc) {
	case 0:
		/*
		 * If the name has been changed, we edit that file, not the
		 * original name.  If the user was editing a temporary file
		 * (or wasn't editing any file), create another one.  The
		 * reason for not reusing temporary files is that there is
		 * special exit processing of them, and reuse is tricky.
		 */
		frp = sp->frp;
		if (sp->ep == NULL || F_ISSET(frp, FR_TMPFILE)) {
			if ((frp = file_add(sp, NULL)) == NULL)
				return (1);
			attach = 0;
		} else
			attach = 1;
		setalt = 0;
		break;
	case 1:
		if ((frp = file_add(sp, cmdp->argv[0]->bp)) == NULL)
			return (1);
		attach = 0;
		setalt = 1;
		set_alt_name(sp, cmdp->argv[0]->bp);
		break;
	default:
		abort();
	}

	if (F_ISSET(cmdp, E_NEWSCREEN))
		return (ex_N_edit(sp, cmdp, frp, attach));

	/* Switch files. */
	if (file_init(sp, frp, NULL, (setalt ? FS_SETALT : 0) |
	    FS_WELCOME | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0)))
		return (1);
	return (0);
}

/*
 * ex_N_edit --
 *	New screen version of ex_edit.
 */
static int
ex_N_edit(sp, cmdp, frp, attach)
	SCR *sp;
	EXCMDARG *cmdp;
	FREF *frp;
	int attach;
{
	SCR *bot, *new, *top;

	/* Get a new screen. */
	if (svi_split(sp, &top, &bot))
		return (1);
	new = sp == top ? bot : top;

	/* Switch files. */
	if (attach) {
		/* Copy file state, keep the screen and cursor the same. */
		new->ep = sp->ep;
		++new->ep->refcnt;

		new->frp = frp;
		new->frp->flags = sp->frp->flags;

		new->lno = sp->lno;
		new->cno = sp->cno;
	} else if (file_init(new, frp, NULL,
	    FS_WELCOME | (F_ISSET(cmdp, E_FORCE) ? FS_FORCE : 0))) {
		if (sp == top)
			(void)svi_join(new, sp, NULL, NULL);
		else
			(void)svi_join(new, NULL, sp, NULL);
		(void)screen_end(new);
		return (1);
	}

	/* Add the new screen to the queue. */
	SIGBLOCK(sp->gp);
	if (sp == bot) {
		/* Split up, link in before the parent. */
		CIRCLEQ_INSERT_BEFORE(&sp->gp->dq, sp, new, q);
	} else {
		/* Split down, link in after the parent. */
		CIRCLEQ_INSERT_AFTER(&sp->gp->dq, sp, new, q);
	}
	SIGUNBLOCK(sp->gp);

	/* Set up the switch. */
	sp->nextdisp = new;
	F_SET(sp, S_SSWITCH);

	return (0);
}
