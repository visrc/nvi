/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_visual.c,v 10.2 1995/05/05 18:53:12 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:53:12 $";
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

#include "common.h"

/*
 * ex_visual -- :[line] vi[sual] [^-.+] [window_size] [flags]
 *	Switch to visual mode.
 *
 * PUBLIC: int ex_visual __P((SCR *, EXCMD *));
 */
int
ex_visual(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
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
	switch (FL_ISSET(cmdp->iflags,
	    E_C_CARAT | E_C_DASH | E_C_DOT | E_C_PLUS)) {
	case E_C_CARAT:
		pos = '^';
		break;
	case E_C_DASH:
		pos = '-';
		break;
	case E_C_DOT:
		pos = '.';
		break;
	case E_C_PLUS:
		pos = '+';
		break;
	default:
		sp->frp->lno = sp->lno;
		sp->frp->cno = 0;
		(void)nonblank(sp, sp->lno, &sp->cno);
		F_SET(sp->frp, FR_CURSORSET);
		goto nopush;
	}

	if (FL_ISSET(cmdp->iflags, E_C_COUNT))
		len = snprintf(buf, sizeof(buf),
		     "%luz%c%lu", sp->lno, pos, cmdp->count);
	else
		len = snprintf(buf, sizeof(buf), "%luz%c", sp->lno, pos);
	(void)v_event_push(sp, buf, len, CH_NOMAP | CH_QUOTED);

	/*
	 * !!!
	 * Historically, if no line address was specified, the [p#l] flags
	 * caused the cursor to be moved to the last line of the file, which
	 * was then positioned as described above.  This seems useless, so
	 * I haven't implemented it.
	 */
	switch (FL_ISSET(cmdp->iflags, E_C_HASH | E_C_LIST | E_C_PRINT)) {
	case E_C_HASH:
		O_SET(sp, O_NUMBER);
		break;
	case E_C_LIST:
		O_SET(sp, O_LIST);
		break;
	case E_C_PRINT:
		break;
	}

	/*
	 * !!!
	 * You can call the visual part of the editor from within an ex
	 * global command.
	 *
	 * XXX
	 * Historically, undoing a visual session was a single undo command,
	 * i.e. you could undo all of the changes you made in visual mode.
	 * We don't get this right; I'm waiting for the new logging code to
	 * be available.
	 */
nopush:	rval = 0;
	F_CLR(sp, S_EX);
	F_SET(sp, S_VI);

	if (F_ISSET(sp, S_EX_GLOBAL)) {
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
#ifdef __TK__
		rval = vs_screen_edit(sp);
#endif
		F_CLR(sp, S_EX | S_VI);
		F_SET(sp, S_EX);
	}
	return (rval);
}
