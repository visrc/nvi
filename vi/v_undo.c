/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.17 1993/02/24 13:02:28 bostic Exp $ (Berkeley) $Date: 1993/02/24 13:02:28 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "log.h"
#include "options.h"
#include "screen.h"
#include "vcmd.h"

/*
 * v_Undo -- U
 *	Undo changes to this line, or roll forward.
 */
int
v_Undo(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (ISSET(O_NUNDO) ?
	    log_forward(ep, rp) : log_backward(ep, rp, SCRLNO(ep)));
}
	
/*
 * v_undo -- u
 *	Undo the last change.
 */
int
v_undo(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	static enum { BACKWARD, FORWARD } last;

	if (ISSET(O_NUNDO))
		return (log_backward(ep, rp, OOBLNO));

	if (!FF_ISSET(ep, F_UNDO)) {
		last = FORWARD;
		FF_SET(ep, F_UNDO);
	}

	switch(last) {
	case BACKWARD:
		if (log_forward(ep, rp)) {
			FF_CLR(ep, F_UNDO);
			return (1);
		}
		last = FORWARD;
		break;
	case FORWARD:
		if (log_backward(ep, rp, OOBLNO)) {
			FF_CLR(ep, F_UNDO);
			return (1);
		}
		last = BACKWARD;
		break;
	}
	return (0);
}
