/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 5.21 1993/05/02 18:30:19 bostic Exp $ (Berkeley) $Date: 1993/05/02 18:30:19 $";
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
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep) == 0)
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
	if ((nlines = file_lline(sp, ep)) == 0) {
		rp->lno = 1;
		rp->cno = 0;
		return (0);
	}

	/*
	 * If deleting lines, leave the cursor at the lowest line deleted,
	 * else, leave the cursor where it started.  Always correct for EOL.
	 *
	 * The historic vi would delete the line the cursor was on (even if
	 * not in line mode) if the motion was past the end-of-file and the
	 * cursor didn't originate on the last line of the file.  We never
	 * delete the line the cursor is on -- we'd have to pass a flag down
	 * to the delete() routine which would have to special case it.
	 */
	if (lmode) {
		rp->lno = MIN(fm->lno, tm->lno);
		if (rp->lno > nlines)
			rp->lno = nlines;
	} else
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
