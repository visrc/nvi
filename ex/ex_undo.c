/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 8.5 1994/03/08 19:39:50 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:39:50 $";
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
 * ex_undol -- U
 *	Undo changes to this line.
 */
int
ex_undol(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (log_setline(sp, ep))
		return (1);

	sp->cno = 0;
	return (0);
}

/*
 * ex_undo -- u
 *	Undo the last change.
 */
int
ex_undo(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	MARK m;

	/*
	 * !!!
	 * Multiple undo isn't available in ex, as there's no '.' command.
	 * Whether 'u' is undo or redo is toggled each time, unless there
	 * was a change since the last undo, in which case it's an undo.
	 */
	if (!F_ISSET(ep, F_UNDO)) {
		F_SET(ep, F_UNDO);
		ep->lundo = FORWARD;
	}
	switch (ep->lundo) {
	case BACKWARD:
		if (log_forward(sp, ep, &m))
			return (1);
		ep->lundo = FORWARD;
		break;
	case FORWARD:
		if (log_backward(sp, ep, &m))
			return (1);
		ep->lundo = BACKWARD;
		break;
	case NOTSET:
		abort();
	}
	sp->lno = m.lno;
	sp->cno = m.cno;
	return (0);
}
