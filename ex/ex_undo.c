/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.13 1993/03/25 15:00:11 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:11 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "log.h"
#include "screen.h"

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
	ep->lno = rp.lno;
	ep->cno = rp.cno;

	F_SET(sp, S_AUTOPRINT);

	return (0);
}
