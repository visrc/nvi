/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 5.11 1992/10/18 13:08:53 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:08:53 $";
#endif /* not lint */

#include <sys/param.h>

#include <limits.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_Delete -- [buffer][count]D
 *	Delete line command.
 */
int
v_Delete(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_char *p;

	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) == 0)
			return (0);
		GETLINE_ERR(fm->lno);
		return (1);
	}

	if (len == 0)
		return (0);

	tm->lno = fm->lno;
	tm->cno = len;

	if (cut(VICB(vp), fm, tm, 0) || delete(fm, tm, 0))
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
v_delete(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t nlines;
	int lmode;
	
	lmode = vp->flags & VC_LMODE;
	if (cut(VICB(vp), fm, tm, lmode) || delete(fm, tm, lmode))
		return (1);

	/*
	 * If deleting lines, leave the cursor at the lowest line deleted,
	 * correcting, of course, for deleting everything.
	 */
	if (lmode) {
		rp->lno = MIN(fm->lno, tm->lno);
		nlines = file_lline(curf);
		if (rp->lno > nlines)
			rp->lno = nlines ? nlines : 1;
		rp->cno = fm->cno;
		return (0);
	}

	/* If deleting forward, leave the cursor in the current spot. */
	if (tm->lno > fm->lno || tm->lno == fm->lno && tm->cno > fm->cno) {
		*rp = *fm;
		return (0);
	}

	/* Else, leave the cursor where it moved. */
	*rp = *tm;
	return (0);
}
