/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_z.c,v 8.6 1993/11/29 14:15:30 bostic Exp $ (Berkeley) $Date: 1993/11/29 14:15:30 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_z -- [count]z[count][-.+^<CR>]
 *	Move the screen.
 */
int
v_z(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
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
		lno = fm->lno;

	/* The second count is the window size. */
	if (F_ISSET(vp, VC_C2SET) && set_window_size(sp, vp->count2, 0))
		return (1);

	/* Set default cursor values. */
	rp->lno = lno;
	rp->cno = fm->cno;

	switch (vp->character) {
	case '-':		/* Put the line at the bottom. */
		if (sp->s_fill(sp, ep, lno, P_BOTTOM))
			return (1);
		break;
	case '.':		/* Put the line in the middle. */
		if (sp->s_fill(sp, ep, lno, P_MIDDLE))
			return (1);
		break;
	default:		/* Put the line at the top for <cr>. */
		value = term_key_val(sp, vp->character);
		if (value != K_CR && value != K_NL) {
			msgq(sp, M_ERR, "usage: %s.", vp->kp->usage);
			return (1);
		}
		/* FALLTHROUGH */
	case '+':		/* Put the line at the top. */
		if (sp->s_fill(sp, ep, lno, P_TOP))
			return (1);
		break;
	case '^':		/* Print the screen before the z- screen. */
		/*
		 * !!!
		 * Historic practice isn't real clear on this one.  It seems
		 * that the command "70z^" is the same as ":70<cr>z-z^" with
		 * an off-by-one difference.  So, until I find documentation
		 * to the contrary, the z^ command in this implementation
		 * displays the screen immediately before the current one.
		 * Fill the screen with the selected line at the bottom, then,
		 * scroll the screen down a page, and move to the middle line
		 * of the screen.  Historic vi moved the cursor to some random
		 * place in the screen, as far as I can tell.
		 */
		if (sp->s_fill(sp, ep, lno, P_BOTTOM))
			return (1);
		if (sp->s_down(sp, ep, rp, sp->t_maxrows - 1, 1))
			return (1);
		if (sp->s_position(sp, ep, rp, 0, P_MIDDLE))
			return (1);
		break;
	}

	/* If the map changes, have to redraw the entire screen. */
	F_SET(sp, S_REDRAW);

	return (0);
}
