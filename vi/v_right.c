/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_right.c,v 5.11 1993/02/16 20:08:44 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:08:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 *
 * Special case: the 'c' and 'd' commands can move past the end of line.
 */
int
v_right(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	u_char *p;

	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) == 0)
			v_eol(ep, NULL);
		else
			GETLINE_ERR(ep, fm->lno);
		return (1);
	}
		
	rp->lno = fm->lno;
	if (len == 0 || fm->cno == len - 1) {
		if (vp->flags & (VC_C|VC_D)) {
			rp->cno = len;
			return (0);
		}
		v_eol(ep, NULL);
		return (1);
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
 * v_dollar -- $
 *	Move to the last column.
 *
 *	One of places that you are allowed to move beyond the end of
 *	the line.
 */
int
v_dollar(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_char *p;

	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) == 0)
			v_eol(ep, NULL);
		else
			GETLINE_ERR(ep, fm->lno);
		return (1);
	}
		
	if (len == 0) {
		v_eol(ep, NULL);
		return (1);
	}

	rp->lno = fm->lno;
	rp->cno = vp->flags & (VC_C | VC_D) ? len : len - 1;
	return (0);
}
