/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_left.c,v 5.3 1992/05/21 12:59:33 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:59:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_left -- [count]^H, [count]h
 *	Move left by columns.
 */
int
v_left(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	u_long cnt;

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	if (cp->cno == 0) {
		bell();
		if (ISSET(O_VERBOSE))
			msg("Already in the first column.");
		return (1);
	}

	rp->lno = cp->lno;
	if (cp->cno > cnt)
		rp->cno = cp->cno - cnt;
	else
		rp->cno = 0;
	return (0);
}

/*
 * v_first -- ^, _
 *	Move to the first non-blank column on this line.
 */
v_first(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	rp->lno = cp->lno;
	return (v_nonblank(rp));
}

/*
 * v_ncol -- [count]|
 *	Move to column count, or the first non-blank column on this line.
 */
v_ncol(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	if (vp->flags & VC_C1SET) {
		rp->lno = cp->lno;
		cp->cno = vp->count;
		return (0);
	}
	rp->lno = cp->lno;
	return (v_nonblank(rp));
}

/*
 * v_zero -- 0
 *	Move to the first column on this line.
 */
v_zero(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	rp->lno = cp->lno;
	rp->cno = 0;
	return (0);
}
