/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_z.c,v 8.16 1994/07/17 13:17:37 bostic Exp $ (Berkeley) $Date: 1994/07/17 13:17:37 $";
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
 * v_z -- [count]z[count][-.+^<CR>]
 *	Move the screen.
 */
int
v_z(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t last, lno;
	u_int value;

	/*
	 * The first count is the line to use.  If the value doesn't
	 * exist, use the last line.
	 */
	if (F_ISSET(vp, VC_C1SET)) {
		lno = vp->count;
		if (file_lline(sp, ep, &last))
			return (1);
		if (lno > last)
			lno = last;
	} else
		lno = vp->m_start.lno;

	/* Set default return cursor values. */
	vp->m_final.lno = lno;
	vp->m_final.cno = vp->m_start.cno;

	/*
	 * The second count is the displayed window size, i.e. the 'z'
	 * command is another way to get artificially small windows.
	 *
	 * !!!
	 * A window size of 0 was historically allowed, and simply ignored.
	 * Also, this could be much more simply done by modifying the value
	 * of the O_WINDOW option, but that's not how it worked historically.
	 */
	if (F_ISSET(vp, VC_C2SET) &&
	    vp->count2 != 0 && sp->s_crel(sp, vp->count2))
		return (1);

	switch (vp->character) {
	case '-':		/* Put the line at the bottom. */
		if (sp->s_fill(sp, ep, lno, P_BOTTOM))
			return (1);
		break;
	case '.':		/* Put the line in the middle. */
		if (sp->s_fill(sp, ep, lno, P_MIDDLE))
			return (1);
		break;
	case '+':
		/*
		 * If the user specified a line number, put that line at the
		 * top and move the cursor to it.  Otherwise, scroll forward
		 * a screen from the current screen.
		 */
		if (F_ISSET(vp, VC_C1SET)) {
			if (sp->s_fill(sp, ep, lno, P_TOP))
				return (1);
			if (sp->s_position(sp, ep, &vp->m_final, 0, P_TOP))
				return (1);
		} else
			if (sp->s_scroll(sp, ep,
			    &vp->m_final, sp->t_rows, Z_PLUS))
				return (1);
		break;
	case '^':
		/*
		 * If the user specified a line number, put that line at the
		 * bottom, move the cursor to it, and then display the screen
		 * before that one.  Otherwise, scroll backward a screen from
		 * the current screen.
		 *
		 * !!!
		 * Note, we match the off-by-one characteristics of historic
		 * vi, here.
		 */
		if (F_ISSET(vp, VC_C1SET)) {
			if (sp->s_fill(sp, ep, lno, P_BOTTOM))
				return (1);
			if (sp->s_position(sp, ep, &vp->m_final, 0, P_TOP))
				return (1);
			if (sp->s_fill(sp, ep, vp->m_final.lno, P_BOTTOM))
				return (1);
		} else
			if (sp->s_scroll(sp, ep,
			    &vp->m_final, sp->t_rows, Z_CARAT))
				return (1);
		break;
	default:		/* Put the line at the top for <cr>. */
		value = KEY_VAL(sp, vp->character);
		if (value != K_CR && value != K_NL) {
			msgq(sp, M_ERR, "usage: %s", vp->kp->usage);
			return (1);
		}
		if (sp->s_fill(sp, ep, lno, P_TOP))
			return (1);
		break;
	}


	return (0);
}
