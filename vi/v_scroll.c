/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_scroll.c,v 5.21 1993/02/19 11:17:44 bostic Exp $ (Berkeley) $Date: 1993/02/19 11:17:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "screen.h"

/*
 * v_lgoto -- [count]G
 *	Go to first non-blank character of the line count, the last line
 *	of the file by default.
 */
int
v_lgoto(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last;

	last = file_lline(ep);
	if (vp->flags & VC_C1SET) {
		if (last < vp->count) {
			v_eof(ep, fm);
			return (1);
		}
		rp->lno = vp->count;
	} else
		rp->lno = last;
	return (0);
}

/* 
 * v_home -- [count]H
 *	Move to the first non-blank character of the line count from
 *	the top of the screen, 1 by default.
 */
int
v_home(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (scr_sm_top(ep,
	    &rp->lno, vp->flags & VC_C1SET ? vp->count : 1));
}

/*
 * v_middle -- M
 *	Move to the first non-blank character of the line in the middle
 *	of the screen.
 */
int
v_middle(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (scr_sm_mid(ep, &rp->lno));
}

/*
 * v_bottom -- [count]L
 *	Move to the first non-blank character of the line count from
 *	the bottom of the screen, 1 by default.
 */
int
v_bottom(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (scr_sm_bot(ep,
	    &rp->lno, vp->flags & VC_C1SET ? vp->count : 1));
}

/*
 * v_up -- [count]^P, [count]k, [count]-
 *	Move up by lines.
 */
int
v_up(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	lno = vp->flags & VC_C1SET ? vp->count : 1;

	if (fm->lno <= lno) {
		v_sof(ep, fm);
		return (1);
	}
	rp->lno = fm->lno - lno;
	return (0);
}

/*
 * v_down -- [count]^J, [count]^N, [count]j, [count]^M, [count]+
 *	Move down by lines.
 */
int
v_down(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;

	lno = fm->lno + (vp->flags & VC_C1SET ? vp->count : 1);

	if (file_gline(ep, lno, &len) == NULL) {
		v_eof(ep, fm);
		return (1);
	}
	rp->lno = lno;
	rp->cno = len ? fm->cno > len - 1 ? len - 1 : fm->cno : 0;
	return (0);
}

/*
 * The historic vi had a problem in that all movements were by physical
 * lines, not by logical, or screen lines.  Arguments can be made that this
 * is the right thing to do.  For example, single line movements, such as
 * 'j' or 'k', should probably work on physical lines.  Commands like "dj",
 * or "j.", where '.' is a change command, make more sense for physical lines
 * than they do logical lines.  The arguments, however, don't apply to
 * scrolling commands like ^D and ^F -- if the window is fairly small, using
 * physical lines can result in a half-page scroll repainting the entire
 * screen, which is not what the user wanted.  This implementation does the
 * scrolling (^B, ^D, ^F, ^U), ^Y and ^E commands using logical lines, not
 * physical.
 */

/*
 * v_hpageup -- [count]^U
 *	Page up half screens.
 */
int
v_hpageup(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	/* 
	 * Half screens always succeed unless already at SOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (vp->flags & VC_C1SET)
		LVAL(O_SCROLL) = vp->count;
	else
		vp->count = LVAL(O_SCROLL);

	return (scr_sm_down(ep, &rp->lno, (recno_t)LVAL(O_SCROLL), 1));
}

/*
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/* 
	 * Half screens always succeed unless already at EOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (vp->flags & VC_C1SET)
		LVAL(O_SCROLL) = vp->count;
	else
		vp->count = LVAL(O_SCROLL);

	return (scr_sm_up(ep, &rp->lno, (recno_t)LVAL(O_SCROLL), 1));
}

/*
 * v_pageup -- [count]^B
 *	Page up full screens.
 */
int
v_pageup(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t count;

	/* Calculation from POSIX 1003.2/D8. */
	count = (vp->flags & VC_C1SET ? vp->count : 1) * (TEXTSIZE(ep) - 1);
	return (scr_sm_down(ep, &rp->lno, count, 1));
}

/*
 * v_pagedown -- [count]^F
 *	Page down full screens.
 */
int
v_pagedown(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t count;

	/* Calculation from POSIX 1003.2/D8. */
	count = (vp->flags & VC_C1SET ? vp->count : 1) * (TEXTSIZE(ep) - 1);
	return (scr_sm_up(ep, &rp->lno, count, 1));
}

/*
 * v_lineup -- [count]^Y
 *	Page up by lines.
 */
int
v_lineup(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t saved_lno;

	saved_lno = ep->lno;

	/*
	 * The cursor moves down, staying with its original line, unless it
	 * reaches the bottom of the screen.  If the line number changes,
	 * we have to do screen relative movement (as if V_RCM was set),
	 * otherwise, we set the relative movement (as if V_RCM_SET was set).
	 * It's enough to make you cry.
	 */
	if (scr_sm_down(ep, &rp->lno, vp->flags & VC_C1SET ? vp->count : 1, 0))
		return (1);
	vp->kp->flags |= rp->lno != saved_lno ? V_RCM : V_RCM_SET;
	return (0);
}

/*
 * v_lineup -- [count]^E
 *	Page up by lines.
 */
int
v_linedown(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t saved_lno;

	saved_lno = ep->lno;

	/*
	 * The cursor moves up, staying with its original line, unless it
	 * reaches the top of the screen.  If the line number changes,
         * we have to do screen relative movement (as if V_RCM was set),
         * otherwise, we set the relative movement (as if V_RCM_SET was set).
         * It's enough to make you cry.
	 */
	if (scr_sm_up(ep, &rp->lno, vp->flags & VC_C1SET ? vp->count : 1, 0))
		return (1);
	vp->kp->flags |= rp->lno != saved_lno ? V_RCM : V_RCM_SET;
	return (0);
}
