/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_undo.c,v 5.13 1992/11/11 18:28:24 bostic Exp $ (Berkeley) $Date: 1992/11/11 18:28:24 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "log.h"
#include "options.h"
#include "screen.h"
#include "extern.h"

#define	LOG_ERR {							\
	bell();								\
	msg("Error: %s/%d: retrieve log error: %s.",			\
	    tail(__FILE__), __LINE__, strerror(errno));			\
	goto err;							\
}

/*
 * v_Undo -- U
 *	Undo changes to this line, or roll forward.
 */
int
v_Undo(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (ISSET(O_NUNDO) ?
	    log_forward(curf, rp) : log_backward(curf, rp, curf->lno));
}
	
/*
 * v_undo -- u
 *	Undo the last change.
 */
int
v_undo(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	static enum { BACKWARD, FORWARD } last = FORWARD;

	if (ISSET(O_NUNDO))
		return (log_backward(curf, rp, OOBLNO));

	switch(last) {
	case BACKWARD:
		last = FORWARD;
		return (log_forward(curf, rp));
	case FORWARD:
		last = BACKWARD;
		return (log_backward(curf, rp, OOBLNO));
	}
	/* NOTREACHED */
}
