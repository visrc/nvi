/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_edit.c,v 8.6 1993/09/09 10:41:22 bostic Exp $ (Berkeley) $Date: 1993/09/09 10:41:22 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_edit --	:e[dit][!] [+cmd] [file]
 *		:vi[sual][!] [+cmd] [file]
 *
 *	Edit a file; if none specified, re-edit the current file.
 *	The second form of the command can only be executed while
 *	in vi mode.  See the hack in ex.c:ex_cmd().
 */
int
ex_edit(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;
	FREF *frp;

	switch (cmdp->argc) {
	case 0:
		frp = sp->frp;
		break;
	case 1:
		if ((frp = file_add(sp, sp->frp, cmdp->argv[0], 1)) == NULL)
			return (1);
		set_alt_fname(sp, cmdp->argv[0]);
		break;
	default:
		abort();
	}

	MODIFY_CHECK(sp, ep, F_ISSET(cmdp, E_FORCE));

	/*
	 * Users don't like "already locked" messages when they do ":e" to
	 * re-edit the current file name.  We handle this here, before the
	 * file_init() call.  If the file_init doesn't succeed we're truly
	 * screwed.  In any case, nobody better try use sp->ep before the
	 * actual switch.
	 */
	if (frp == sp->frp) {
		if (file_end(sp, sp->ep, F_ISSET(cmdp, E_FORCE)))
			return (1);
		sp->ep = NULL;
	}

	/* Switch files. */
	if ((tep = file_init(sp, NULL, frp, NULL)) == NULL)
		return (1);
	sp->n_ep = tep;
	sp->n_frp = frp;
	F_SET(sp, F_ISSET(cmdp, E_FORCE) ? S_FSWITCH_FORCE : S_FSWITCH);
	return (0);
}

/*
 * ex_visual -- :[line] vi[sual] [^-.+] [window_size] [flags]
 *
 *	Switch to visual mode.
 */
int
ex_visual(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	size_t len;
	int pos;
	char buf[256];

	/* If a line specified, move to that line. */
	if (cmdp->addrcnt)
		sp->lno = cmdp->addr1.lno;

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
	default:
		pos = '+';
		break;
	}

	if (F_ISSET(cmdp, E_COUNT))
		len = snprintf(buf, sizeof(buf),
		     "%luz%lu%c", sp->lno, pos, cmdp->count);
	else
		len = snprintf(buf, sizeof(buf), "%luz%c", sp->lno, pos);
	(void)term_push(sp, &sp->tty, buf, len);

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

	/* Switch modes. */
	F_CLR(sp, S_MODE_EX);
	F_SET(sp, S_MODE_VI);

	return (0);
}
