/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_xchar.c,v 8.3 1993/11/04 16:17:27 bostic Exp $ (Berkeley) $Date: 1993/11/04 16:17:27 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

#define	NODEL(sp) {							\
	msgq(sp, M_BERR, "No characters to delete.");			\
	return (1);							\
}

/*
 * v_xchar --
 *	Deletes the character(s) on which the cursor sits.
 */
int
v_xchar(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK m;
	recno_t lno;
	u_long cnt;
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			NODEL(sp);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		NODEL(sp);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	/*
	 * Deleting from the cursor toward the end of line, w/o moving the
	 * cursor.  Note, "2x" at EOL isn't the same as "xx" because the
	 * left movement of the cursor as part of the 'x' command isn't
	 * taken into account.  Historically correct.
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

	if (cut(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? vp->buffer : DEFCB, fm, tm, 0))
		return (1);
	if (delete(sp, ep, fm, tm, 0))
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
v_Xchar(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;

	if (fm->cno == 0) {
		msgq(sp, M_BERR, "Already at the left-hand margin.");
		return (1);
	}

	*tm = *fm;
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	fm->cno = cnt >= tm->cno ? 0 : tm->cno - cnt;

	if (cut(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? vp->buffer : DEFCB, fm, tm, 0))
		return (1);
	if (delete(sp, ep, fm, tm, 0))
		return (1);

	*rp = *fm;
	return (0);
}
