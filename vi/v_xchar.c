/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_xchar.c,v 8.6 1994/03/08 19:41:44 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:41:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_xchar -- [count]x
 *	Deletes the character(s) on which the cursor sits.
 */
int
v_xchar(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	size_t len;

	if (file_gline(sp, ep, vp->m_start.lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			goto nodel;
		GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}
	if (len == 0) {
nodel:		msgq(sp, M_BERR, "No characters to delete.");
		return (1);
	}

	/*
	 * Delete from the cursor toward the end of line, w/o moving the
	 * cursor.
	 *
	 * !!!
	 * Note, "2x" at EOL isn't the same as "xx" because the left movement
	 * of the cursor as part of the 'x' command isn't taken into account.
	 * Historically correct.
	 */
	if (F_ISSET(vp, VC_C1SET))
		vp->m_stop.cno += vp->count - 1;
	if (vp->m_stop.cno >= len - 1) {
		vp->m_stop.cno = len - 1;
		vp->m_final.cno = vp->m_start.cno ? vp->m_start.cno - 1 : 0;
	} else
		vp->m_final.cno = vp->m_start.cno;

	if (cut(sp, ep, NULL,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, 0))
		return (1);
	return (delete(sp, ep, &vp->m_start, &vp->m_stop, 0));
}

/*
 * v_Xchar -- [count]X
 *	Deletes the character(s) immediately before the current cursor
 *	position.
 */
int
v_Xchar(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	u_long cnt;

	if (vp->m_start.cno == 0) {
		v_sol(sp);
		return (1);
	}

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if (cnt >= vp->m_start.cno)
		vp->m_start.cno = 0;
	else
		vp->m_start.cno -= cnt;
	--vp->m_stop.cno;
	vp->m_final.cno = vp->m_start.cno;

	if (cut(sp, ep, NULL,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, 0))
		return (1);
	return (delete(sp, ep, &vp->m_start, &vp->m_stop, 0));
}
