/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_right.c,v 8.3 1993/12/16 12:04:11 bostic Exp $ (Berkeley) $Date: 1993/12/16 12:04:11 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 *
 * Special case: the 'c' and 'd' commands can move past the end of line.
 */
int
v_right(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, fm->lno);
		return (1);
	}
		
	rp->lno = fm->lno;
	if (len == 0 || fm->cno == len - 1) {
		if (F_ISSET(vp, VC_C | VC_D | VC_Y)) {
			rp->cno = len;
			return (0);
		}
		v_eol(sp, ep, NULL);
		return (1);
	}

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	rp->cno = fm->cno + cnt;
	if (rp->cno > len - 1)
		if (F_ISSET(vp, VC_C | VC_D | VC_Y))
			rp->cno = len;
		else
			rp->cno = len - 1;
	return (0);
}

/*
 * v_dollar -- [count]$
 *	Move to the last column.
 */
int
v_dollar(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;
	u_long cnt;

	/*
	 * A count moves down count - 1 rows, so, "3$" is the same as "2j$".
	 *
	 * !!!
	 * Historically, if the $ is a motion, and deleting from at or before
	 * the first non-blank of the line, it's a line motion, and the line
	 * motion flag is set.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if (cnt != 1) {
		--vp->count;
		if (v_down(sp, ep, vp, fm, tm, rp))
			return (1);
		rp->cno = 0;
		if (nonblank(sp, ep, rp->lno, &rp->cno))
			return (1);
		if (fm->cno <= rp->cno && F_ISSET(vp, VC_C | VC_D | VC_Y))
			F_SET(vp, VC_LMODE);
	} else
		rp->lno = fm->lno;

	if (file_gline(sp, ep, rp->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, rp->lno);
		return (1);
	}
		
	if (len == 0) {
		v_eol(sp, ep, NULL);
		return (1);
	}

	/* If it's a motion component, move one past the end of the line. */
	rp->cno = F_ISSET(vp, VC_C | VC_D | VC_Y) ? len : len - 1;
	return (0);
}
