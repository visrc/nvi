/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 10.3 1995/09/21 10:58:06 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:58:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

/*
 * ex_undo -- u
 *	Undo the last change.
 *
 * PUBLIC: int ex_undo __P((SCR *, EXCMD *));
 */
int
ex_undo(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EXF *ep;
	MARK m;

	/*
	 * !!!
	 * Historic undo always set the previous context mark.
	 */
	m.lno = sp->lno;
	m.cno = sp->cno;
	if (mark_set(sp, ABSMARK1, &m, 1))
		return (1);

	/*
	 * !!!
	 * Multiple undo isn't available in ex, as there's no '.' command.
	 * Whether 'u' is undo or redo is toggled each time, unless there
	 * was a change since the last undo, in which case it's an undo.
	 */
	ep = sp->ep;
	if (!F_ISSET(ep, F_UNDO)) {
		F_SET(ep, F_UNDO);
		ep->lundo = FORWARD;
	}
	switch (ep->lundo) {
	case BACKWARD:
		if (log_forward(sp, &m))
			return (1);
		ep->lundo = FORWARD;
		break;
	case FORWARD:
		if (log_backward(sp, &m))
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
