/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_xchar.c,v 5.14 1993/02/19 18:37:41 bostic Exp $ (Berkeley) $Date: 1993/02/19 18:37:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"

#define	NODEL(ep) {							\
	msg(ep, M_BELL, "No characters to delete.");			\
	return (1);							\
}

/*
 * v_xchar --
 *	Deletes the character(s) on which the cursor sits.
 */
int
v_xchar(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK m;
	u_long cnt;
	size_t len;

	if (file_gline(ep, fm->lno, &len) == NULL) {
		if (file_lline(ep) == 0)
			NODEL(ep);
		GETLINE_ERR(ep, fm->lno);
		return (1);
	}

	if (len == 0)
		NODEL(ep);

	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	fm->lno = tm->lno = fm->lno;

	/*
	 * Deleting from the cursor toward the end of line, w/o moving the
	 * cursor.  Note, "2x" at EOL isn't the same as "xx" because the
	 * left movement of the cursor as part of the 'x' command is't taken
	 * into account.  Historically correct.
	 */
	tm->lno = fm->lno;
	if (cnt < len - fm->cno) {
		tm->cno = fm->cno + cnt;
		m = *fm;
	} else {
		tm->cno = len;
		m.lno = fm->lno;
		m.cno = fm->cno ? fm->cno - 1 : 0;
	}

	if (cut(ep, VICB(vp), fm, tm, 0) || delete(ep, fm, tm, 0))
		return (1);

	*rp = m;
	return (0);
}

/*
 * v_Xchar --
 *	Deletes the character(s) immediately before the current cursor
 *	position.
 */
int
v_Xchar(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;

	if (fm->cno == 0) {
		msg(ep, M_BELL, "Already at the left-hand margin.");
		return (1);
	}

	*tm = *fm;
	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	fm->cno = cnt >= tm->cno ? 0 : tm->cno - cnt;

	if (cut(ep, VICB(vp), fm, tm, 0) || delete(ep, fm, tm, 0))
		return (1);

	*rp = *fm;
	return (0);
}
