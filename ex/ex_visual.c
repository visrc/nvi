/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_visual.c,v 9.6 1995/02/08 15:30:38 bostic Exp $ (Berkeley) $Date: 1995/02/08 15:30:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
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

/*
 * ex_visual -- :[line] vi[sual] [^-.+] [window_size] [flags]
 *
 *	Switch to visual mode.
 */
int
ex_visual(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	size_t len;
	int pos, rval;
	char buf[256];

	/* If open option off, disallow visual command. */
	if (!O_ISSET(sp, O_OPEN)) {
		msgq(sp, M_ERR,
	    "171|The visual command requires that the open option be set");
		return (1);
	}

	/* Move to the address. */
	sp->lno = cmdp->addr1.lno == 0 ? 1 : cmdp->addr1.lno;

	/*
	 * Push a command based on the line position flags.  If no
	 * flag specified, the line goes at the top of the screen.
	 */
	switch (F_ISSET(cmdp, E_F_CARAT | E_F_DASH | E_F_DOT | E_F_PLUS)) {
	case E_F_CARAT:
		pos = '^';
		break;
	case E_F_DASH:
		pos = '-';
		break;
	case E_F_DOT:
		pos = '.';
		break;
	case E_F_PLUS:
		pos = '+';
		break;
	default:
		sp->frp->lno = sp->lno;
		sp->frp->cno = 0;
		F_SET(sp->frp, FR_CURSORSET | FR_FNONBLANK);
		goto nopush;
	}

	if (F_ISSET(cmdp, E_COUNT))
		len = snprintf(buf, sizeof(buf),
		     "%luz%c%lu", sp->lno, pos, cmdp->count);
	else
		len = snprintf(buf, sizeof(buf), "%luz%c", sp->lno, pos);
	(void)term_push(sp, buf, len, CH_NOMAP | CH_QUOTED);

	/*
	 * !!!
	 * Historically, if no line address was specified, the [p#l] flags
	 * caused the cursor to be moved to the last line of the file, which
	 * was then positioned as described above.  This seems useless, so
	 * I haven't implemented it.
	 */
	switch (F_ISSET(cmdp, E_F_HASH | E_F_LIST | E_F_PRINT)) {
	case E_F_HASH:
		O_SET(sp, O_NUMBER);
		break;
	case E_F_LIST:
		O_SET(sp, O_LIST);
		break;
	case E_F_PRINT:
		break;
	}

	/*
	 * !!!
	 * You can call the visual part of the editor from within an ex
	 * global command.
	 */
nopush:	rval = 0;
	F_CLR(sp, S_SCREENS);
	F_SET(sp, sp->saved_vi_mode);

	if (F_ISSET(sp, S_GLOBAL)) {
		/*
		 * When the vi screen(s) exit, we don't want to lose our hold
		 * on this screen or this file, otherwise we're going to fail
		 * fairly spectacularly.
		 */
		++sp->refcnt;
		++sp->ep->refcnt;

		/*
		 * !!!
		 * Historically, if the user exited the vi screen(s) using an
		 * ex quit command (e.g. :wq, :q) ex/vi exited, it was only if
		 * they exited vi using the Q command that ex continued.  We
		 * continue in ex regardless, based on the belief that Q isn't
		 * that intuitive for most users and the intent is clear.
		 */
		rval = svi_screen_edit(sp);
		F_CLR(sp, S_SCREENS);
		F_SET(sp, S_EX);
	}
	return (rval);
}
