/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_undo.c,v 5.11 1993/02/16 20:10:29 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:29 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "log.h"

int
ex_undo(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	static enum { BACKWARD, FORWARD } last = FORWARD;
	MARK rp;

	FF_SET(ep, F_AUTOPRINT);

	switch(last) {
	case BACKWARD:
		last = FORWARD;
		if (log_forward(ep, &rp))
			return (1);
		break;
	case FORWARD:
		last = BACKWARD;
		if (log_backward(ep, &rp, OOBLNO))
			return (1);
		break;
	}
	ep->lno = rp.lno;
	ep->cno = rp.cno;
	return (0);
}
