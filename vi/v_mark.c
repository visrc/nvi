/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 10.3 1995/09/21 10:59:00 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:59:00 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

/*
 * v_mark -- m[a-z]
 *	Set a mark.
 *
 * PUBLIC: int v_mark __P((SCR *, VICMD *));
 */
int
v_mark(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (mark_set(sp, vp->character, &vp->m_start, 1));
}

enum which {BQMARK, FQMARK};
static int mark __P((SCR *, VICMD *, enum which));


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
 *
 * PUBLIC: int v_bmark __P((SCR *, VICMD *));
 */
int
v_bmark(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (mark(sp, vp, BQMARK));
}

/*
 * v_fmark -- '['`a-z]
 *	Move to a mark.
 *
 * Move to the first nonblank character of the line containing the mark.
 *
 * PUBLIC: int v_fmark __P((SCR *, VICMD *));
 */
int
v_fmark(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (mark(sp, vp, FQMARK));
}

/*
 * mark --
 *	Mark corrections.
 */
static int
mark(sp, vp, cmd)
	SCR *sp;
	VICMD *vp;
	enum which cmd;
{
	dir_t dir;
	MARK m;
	size_t len;

	if (mark_get(sp, vp->character, &vp->m_stop, M_BERR))
		return (1);

	/*
	 * !!!
	 * Historically, BQMARKS for character positions that no longer
	 * existed acted as FQMARKS.
	 *
	 * FQMARKS move to the first non-blank.
	 */
	switch (cmd) {
	case BQMARK:
		if (file_gline(sp, vp->m_stop.lno, &len) == NULL) {
			FILE_LERR(sp, vp->m_stop.lno);
			return (1);
		}
		if (vp->m_stop.cno < len ||
		    vp->m_stop.cno == len && len == 0)
			break;

		if (ISMOTION(vp))
			F_SET(vp, VM_LMODE);
		cmd = FQMARK;
		/* FALLTHROUGH */
	case FQMARK:
		vp->m_stop.cno = 0;
		if (nonblank(sp, vp->m_stop.lno, &vp->m_stop.cno))
			return (1);
		break;
	default:
		abort();
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
	 * If the motion is in the reverse direction, switch the start and
	 * stop MARK's so that it's in a forward direction.  (There's no
	 * reason for this other than to make the tests below easier.  The
	 * code in vi.c:vi() would have done the switch.)  Both forward
	 * and backward motions can happen for any kind of search command
	 * because of the wrapscan option.
	 */
	if (vp->m_start.lno > vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno > vp->m_stop.cno) {
		m = vp->m_start;
		vp->m_start = vp->m_stop;
		vp->m_stop = m;
		dir = BACKWARD;
	} else
		dir = FORWARD;

	/*
	 * Yank cursor motion, when associated with marks as motion commands,
	 * historically behaved as follows:
	 *
	 * ` motion			' motion
	 *		Line change?		Line change?
	 *		Y	N		Y	N
	 *	      --------------	      ---------------
	 * FORWARD:  |	NM	NM	      | NM	NM
	 *	     |			      |
	 * BACKWARD: |	M	M	      | M	NM(1)
	 *
	 * where NM means the cursor didn't move, and M means the cursor
	 * moved to the mark.
	 *
	 * As the cursor was usually moved for yank commands associated
	 * with backward motions, this implementation regularizes it by
	 * changing the NM at position (1) to be an M.  This makes mark
	 * motions match search motions, which is probably A Good Thing.
	 *
	 * Delete cursor motion was always to the start of the text region,
	 * regardless.  Ignore other motion commands.
	 */
#ifdef HISTORICAL_PRACTICE
	if (ISCMD(vp->rkp, 'y')) {
		if ((cmd == BQMARK ||
		    cmd == FQMARK && vp->m_start.lno != vp->m_stop.lno) &&
		    (vp->m_start.lno > vp->m_stop.lno ||
		    vp->m_start.lno == vp->m_stop.lno &&
		    vp->m_start.cno > vp->m_stop.cno))
			vp->m_final = vp->m_stop;
	} else if (ISCMD(vp->rkp, 'd'))
		if (vp->m_start.lno > vp->m_stop.lno ||
		    vp->m_start.lno == vp->m_stop.lno &&
		    vp->m_start.cno > vp->m_stop.cno)
			vp->m_final = vp->m_stop;
#else
	vp->m_final = vp->m_start;
#endif

	/*
	 * Forward marks are always line oriented, and it's set in the
	 * vcmd.c table.
	 */
	if (cmd == FQMARK)
		return (0);

	/*
	 * BQMARK'S moving backward and starting at column 0, and ones moving
	 * forward and ending at column 0 are corrected to the last column of
	 * the previous line.  Otherwise, adjust the starting/ending point to
	 * the character before the current one (this is safe because we know
	 * the search had to move to succeed).
	 *
	 * Mark motions become line mode opertions if they start at column 0
	 * and end at column 0 of another line.
	 */
	if (vp->m_start.lno < vp->m_stop.lno && vp->m_stop.cno == 0) {
		if (file_gline(sp, --vp->m_stop.lno, &len) == NULL) {
			FILE_LERR(sp, vp->m_stop.lno);
			return (1);
		}
		if (vp->m_start.cno == 0)
			F_SET(vp, VM_LMODE);
		vp->m_stop.cno = len ? len - 1 : 0;
	} else
		--vp->m_stop.cno;

	return (0);
}
