/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 8.4 1993/09/01 16:28:22 bostic Exp $ (Berkeley) $Date: 1993/09/01 16:28:22 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Delete -- [buffer][count]D
 *	Delete line command.
 */
int
v_Delete(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			return (0);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		return (0);

	tm->lno = fm->lno;
	tm->cno = len;

	if (cut(sp, ep, VICB(vp), fm, tm, 0) || delete(sp, ep, fm, tm, 0))
		return (1);

	rp->lno = fm->lno;
	rp->cno = fm->cno ? fm->cno - 1 : 0;
	return (0);
}

/*
 * v_delete -- [buffer][count]d[count]motion
 *	Delete a range of text.
 */
int
v_delete(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t nlines;
	size_t len;
	int lmode;
	
	lmode = F_ISSET(vp, VC_LMODE);
	if (cut(sp, ep, VICB(vp), fm, tm, lmode) ||
	    delete(sp, ep, fm, tm, lmode))
		return (1);

	/* Check for deleting the file. */
	if (file_lline(sp, ep, &nlines))
		return (1);
	if (nlines == 0) {
		rp->lno = 1;
		rp->cno = 0;
		return (0);
	}

	/*
	 * If deleting lines, leave the cursor at the lowest line deleted,
	 * else, leave the cursor where it started.  Always correct for EOL.
	 *
	 * The historic vi would delete the line the cursor was on (even if
	 * not in line mode) if the motion from the cursor was past the EOF
	 * and the cursor didn't originate on the last line of the file.  A
	 * strange special case.  We never delete the line the cursor is on.
	 * We'd have to pass a flag down to the delete() routine which would
	 * have to special case it.
	 */
	if (lmode) {
		rp->lno = MIN(fm->lno, tm->lno);
		if (rp->lno > nlines)
			rp->lno = nlines;
		rp->cno = 0;
		(void)nonblank(sp, ep, rp->lno, &rp->cno);
		return (0);
	}

	rp->lno = fm->lno;
	if (file_gline(sp, ep, rp->lno, &len) == NULL) {
		GETLINE_ERR(sp, rp->lno);
		return (1);
	}
	if (fm->cno >= len)
		rp->cno = len ? len - 1 : 0;
	else
		rp->cno = fm->cno;
	return (0);
}
