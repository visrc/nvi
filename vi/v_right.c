/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_right.c,v 5.5 1992/10/10 14:02:28 bostic Exp $ (Berkeley) $Date: 1992/10/10 14:02:28 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

#define	EOLERR {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("Already at the end of the line.");			\
	return (1);							\
}

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 *
 * Special case: the 'c' and 'd' commands can move past the end of line.
 */
int
v_right(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	u_char *p;

	EGETLINE(p, fm->lno, len);

	rp->lno = fm->lno;
	if (len == 0 || fm->cno == len - 1) {
		if (vp->flags & (VC_C|VC_D)) {
			rp->cno = len;
			return (0);
		}
		EOLERR;
	}

	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	rp->cno = fm->cno + cnt;
	if (rp->cno > len - 1)
		if (vp->flags & (VC_C|VC_D))
			rp->cno = len;
		else
			rp->cno = len - 1;
	return (0);
}

/*
 * v_eol -- $
 *	Move to the last column.
 *
 *	One of places that you are allowed to move beyond the end of
 *	the line.
 */
int
v_eol(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_char *p;

	EGETLINE(p, fm->lno, len);

	if (len == 0)
		EOLERR;

	rp->lno = fm->lno;
	rp->cno = vp->flags & VC_ISMOTION ? len : len - 1;
	return (0);
}
