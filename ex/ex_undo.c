/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.18 1993/05/17 17:25:55 bostic Exp $ (Berkeley) $Date: 1993/05/17 17:25:55 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_undol -- U
 *	Undo changes to this line, or roll forward.
 */
int
ex_undol(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	MARK m;

	if (O_ISSET(sp, O_NUNDO)) {
		if (log_forward(sp, ep, &m))
			return (1);
	} else {
		if (log_setline(sp, ep, &m))
			return (1);
	}

	sp->lno = m.lno;
	sp->cno = m.cno;

	F_SET(sp, S_AUTOPRINT);

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

	if (O_ISSET(sp, O_NUNDO)) {
		if (log_backward(sp, ep, &m))
			return (1);
	} else {
		if (!F_ISSET(ep, F_UNDO)) {
			ep->lundo = UFORWARD;
			F_SET(ep, F_UNDO);
		}

		switch (ep->lundo) {
		case UBACKWARD:
			if (log_forward(sp, ep, &m)) {
				F_CLR(ep, F_UNDO);
				return (1);
			}
			ep->lundo = UFORWARD;
			break;
		case UFORWARD:
			if (log_backward(sp, ep, &m)) {
				F_CLR(ep, F_UNDO);
				return (1);
			}
			ep->lundo = UBACKWARD;
			break;
		}
	}
	sp->lno = m.lno;
	sp->cno = m.cno;

	F_SET(sp, S_AUTOPRINT);

	return (0);
}
