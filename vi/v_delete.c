/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 8.14 1994/07/28 12:37:41 bostic Exp $ (Berkeley) $Date: 1994/07/28 12:37:41 $";
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
 * v_Delete -- [buffer][count]D
 *	Delete line command.
 */
int
v_Delete(sp, ep, vp)
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
			return (0);
		GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	if (len == 0)
		return (0);

	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno = len - 1;

	/* Yank the lines. */
	if (cut(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, CUT_NUMOPT))
		return (1);
	if (delete(sp, ep, &vp->m_start, &vp->m_stop, 0))
		return (1);

	vp->m_final.lno = vp->m_start.lno;
	vp->m_final.cno = vp->m_start.cno ? vp->m_start.cno - 1 : 0;
	return (0);
}

/*
 * v_delete -- [buffer][count]d[count]motion
 *	Delete a range of text.
 */
int
v_delete(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t nlines;
	size_t len;
	int lmode;

	lmode = F_ISSET(vp, VM_LMODE) ? CUT_LINEMODE : 0;

	/* Yank the lines. */
	if (cut(sp, ep, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop,
	    lmode | (F_ISSET(vp, VM_CUTREQ) ? CUT_NUMREQ : CUT_NUMOPT)))
		return (1);

	/* Delete the lines. */
	if (delete(sp, ep, &vp->m_start, &vp->m_stop, lmode))
		return (1);

	/*
	 * Check for deletion of the entire file.  Try to check a close
	 * by line so we don't go to the end of the file unnecessarily.
	 */
	if (file_gline(sp, ep, vp->m_final.lno + 1, &len) == NULL) {
		if (file_lline(sp, ep, &nlines))
			return (1);
		if (nlines == 0) {
			vp->m_final.lno = 1;
			vp->m_final.cno = 0;
			return (0);
		}
	}

	/*
	 * One special correction, in case we've deleted the current line or
	 * character.  We check it here instead of checking in every command
	 * that can be a motion component.
	 */
	if (file_gline(sp, ep, vp->m_final.lno, &len) == NULL) {
		if (file_gline(sp, ep, nlines, &len) == NULL) {
			GETLINE_ERR(sp, nlines);
			return (1);
		}
		vp->m_final.lno = nlines;
	}
	if (vp->m_final.cno >= len)
		vp->m_final.cno = len ? len - 1 : 0;

	/*
	 * !!!
	 * The "dd" command moved to the first non-blank; "d<motion>" didn't.
	 */
	if (F_ISSET(vp, VM_LDOUBLE)) {
		F_CLR(vp, VM_RCM_MASK);
		F_SET(vp, VM_RCM_SETFNB);
	}
	return (0);
}
