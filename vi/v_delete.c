/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 9.2 1994/11/12 11:22:15 bostic Exp $ (Berkeley) $Date: 1994/11/12 11:22:15 $";
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
 * v_delete -- [buffer][count]d[count]motion
 *	       [buffer][count]D
 *	Delete a range of text.
 */
int
v_delete(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	recno_t nlines;
	size_t len;
	int lmode;

	lmode = F_ISSET(vp, VM_LMODE) ? CUT_LINEMODE : 0;

	/* Yank the lines. */
	if (cut(sp, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop,
	    lmode | (F_ISSET(vp, VM_CUTREQ) ? CUT_NUMREQ : CUT_NUMOPT)))
		return (1);

	/* Delete the lines. */
	if (delete(sp, &vp->m_start, &vp->m_stop, lmode))
		return (1);

	/*
	 * Check for deletion of the entire file.  Try to check a close
	 * by line so we don't go to the end of the file unnecessarily.
	 */
	if (file_gline(sp, vp->m_final.lno + 1, &len) == NULL) {
		if (file_lline(sp, &nlines))
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
	if (file_gline(sp, vp->m_final.lno, &len) == NULL) {
		if (file_gline(sp, nlines, &len) == NULL) {
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
