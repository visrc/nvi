/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.15 1993/04/05 07:11:52 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:52 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

int
ex_undo(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	MARK rp;

	if (!F_ISSET(ep, F_UNDO)) {
		ep->lundo = UFORWARD;
		F_SET(ep, F_UNDO);
	}

	switch(ep->lundo) {
	case UBACKWARD:
		ep->lundo = UFORWARD;
		if (log_forward(sp, ep, &rp))
			return (1);
		break;
	case UFORWARD:
		ep->lundo = UBACKWARD;
		if (log_backward(sp, ep, &rp, OOBLNO))
			return (1);
		break;
	}
	sp->lno = rp.lno;
	sp->cno = rp.cno;

	F_SET(sp, S_AUTOPRINT);

	return (0);
}
