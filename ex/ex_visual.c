/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_visual.c,v 8.4 1993/09/30 12:02:49 bostic Exp $ (Berkeley) $Date: 1993/09/30 12:02:49 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

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
		     "%luz%c%lu", sp->lno, pos, cmdp->count);
	else
		len = snprintf(buf, sizeof(buf), "%luz%c", sp->lno, pos);
	(void)term_push(sp, sp->gp->tty, buf, len);

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
