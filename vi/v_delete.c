/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 5.5 1992/05/21 12:57:39 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:57:39 $";
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
v_delete(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	int buffer, lmode;
	
	lmode = vp->flags & VC_LMODE;
	buffer = vp->buffer == OOBCB ? DEFCB : vp->buffer;

	if (cut(buffer, cp, &vp->motion, lmode) ||
	    delete(cp, &vp->motion, lmode))
		return (1);

	/* If deleting lines, leave the cursor at the lowest line deleted. */
	if (lmode) {
		rp->lno = MIN(cp->lno, vp->motion.lno);
		rp->cno = cp->cno;
		return (0);
	}

	/* If deleting forward, leave the cursor in the current spot. */
	if (vp->motion.lno > cp->lno ||
	    vp->motion.lno == cp->lno && vp->motion.cno > cp->cno) {
		*rp = *cp;
		return (0);
	}

	/* Else, leave the cursor where it moved. */
	*rp = vp->motion;
	return (0);
}
