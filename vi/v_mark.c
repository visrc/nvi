/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 8.5 1994/03/04 18:03:53 bostic Exp $ (Berkeley) $Date: 1994/03/04 18:03:53 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_mark -- m[a-z]
 *	Set a mark.
 */
int
v_mark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark_set(sp, ep, vp->character, &vp->m_start, 1));
}

static int mark __P((SCR *, EXF *, VICMDARG *, enum direction));

/*
 * v_bmark -- `['`a-z]
 *	Move to a mark.
 *
 * Moves to a mark, setting both row and column.
 *
 * !!!
 * Although not commonly known, the "'`" and "'`" forms are historically
 * valid.  The behavior is determined by the first character, so "`'" is
 * the same as "``".  Remember this fact -- you'll be amazed at how many
 * people don't know it and will be delighted that you are able to tell
 * them.
 */
int
v_bmark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark(sp, ep, vp, BACKWARD));
}

/*
 * v_fmark -- '['`a-z]
 *	Move to a mark.
 *
 * Move to the first nonblank character of the line containing the mark.
 */
int
v_fmark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark(sp, ep, vp, FORWARD));
}

static int
mark(sp, ep, vp, dir)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	enum direction dir;
{
	if (mark_get(sp, ep, vp->character, &vp->m_stop))
		return (1);

	/* Forward marks move to the first non-blank. */
	if (dir == FORWARD) {
		vp->m_stop.cno = 0;
		if (nonblank(sp, ep, vp->m_stop.lno, &vp->m_stop.cno))
			return (1);
	}

	/* Non-motion commands move to the end of the range. */
	if (!ISMOTION(vp)) {
		vp->m_final = vp->m_stop;
		return (0);
	}

	/*
	 * !!!
	 * If a motion component, the cursor has to move.
	 */
	if (vp->m_stop.lno == vp->m_start.lno &&
	    vp->m_stop.cno == vp->m_start.cno) {
		v_nomove(sp);
		return (1);
	}

	/*
	 * If moving right, VC_D and VC_Y stay at the start.  If moving left,
	 * VC_D commands move to the end of the range and VC_Y remains at the
	 * start.  Ignore VC_C and VC_S.  Motion left commands adjust the
	 * starting point to the character before the current one.
	 */
	if (vp->m_start.lno > vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno > vp->m_stop.cno) {
		if (F_ISSET(vp, VC_D))
			vp->m_final = vp->m_stop;
		else
			vp->m_final = vp->m_start;
		--vp->m_start.cno;
	} else
		vp->m_final = vp->m_start;
	return (0);
}
