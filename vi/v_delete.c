/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 5.6 1992/05/27 10:35:25 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:35:25 $";
#endif /* not lint */

#include <sys/param.h>
#include <limits.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "extern.h"

/*
 * v_delete --
 *	Delete a range of text.
 */
int
v_delete(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int buffer, lmode;
	
	lmode = vp->flags & VC_LMODE;
	buffer = vp->buffer == OOBCB ? DEFCB : vp->buffer;

	if (cut(buffer, fm, tm, lmode) || delete(fm, tm, lmode))
		return (1);

	/* If deleting lines, leave the cursor at the lowest line deleted. */
	if (lmode) {
		rp->lno = MIN(fm->lno, tm->lno);
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
