/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_scroll.c,v 5.20 1993/02/16 20:08:20 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:08:20 $";
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

#define	NO_SUCH_LINE(ep) {						\
	msg(ep, M_ERROR, "No such line on the screen.");		\
	return (1);							\
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
	if (scr_smtop(ep, &rp->lno, vp->flags & VC_C1SET ? vp->count : 1))
		NO_SUCH_LINE(ep);
	return (0);
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
	if (scr_smmid(ep, &rp->lno))
		NO_SUCH_LINE(ep);
	return (0);
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
	if (scr_smbot(ep, &rp->lno, vp->flags & VC_C1SET ? vp->count : 1))
		NO_SUCH_LINE(ep);
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
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last, lno, top;

	/* 
	 * Half screens always succeed unless already at EOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (vp->flags & VC_C1SET)
		LVAL(O_SCROLL) = vp->count;
	else
		vp->count = LVAL(O_SCROLL);

	/* Check for EOF. */
	last = file_lline(ep);
	if (fm->lno > last) {
		v_eof(ep, NULL);
		return (1);
	}

	/*
	 * Figure out the new top line.  Increment by the suggested amount.
	 * If it's past the EOF, calculate a new one, but don't make any
	 * change if the top line would be decremented (may be on a partial
	 * screen).
	 */
	top = ep->top + vp->count;
	if (last < BOTLINE(ep, top)) {
		if (last > SCREENSIZE(ep)) {
			top = last - SCREENSIZE(ep) + 1;
			if (top > ep->top)
				ep->top = top;
		}
	} else
		ep->top = top;

	/*
	 * Figure out the new cursor line.  Increment by the suggested amount.
	 * If it's past the EOF, calculate a new one.
	 */
	lno = fm->lno + vp->count;
	rp->lno = last < lno ? last : lno;
	return (0);
}

/*
 * v_pagedown -- [count]^F
 *	Page down by screens.
 */
int
v_pagedown(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t cnt, last, lno, top;

	/* Check for EOF. */
	last = file_lline(ep);
	if (fm->lno > last) {
		v_eof(ep, NULL);
		return (1);
	}

	/*
	 * Figure out the new top line.  Increment by the suggested amount.
	 * If it's past the EOF, calculate a new one, but don't make any
	 * change if the top line would be decremented (may be on a partial
	 * screen).
	 * Calculation from POSIX 1003.2/D8.
	 */
	cnt = (vp->flags & VC_C1SET ? vp->count : 1) * (ep->lines - 3);
	top = ep->top + cnt;
	if (last < BOTLINE(ep, top)) {
		if (last > SCREENSIZE(ep)) {
			top = last - SCREENSIZE(ep) + 1;
			if (top > ep->top)
				ep->top = top;
		}
	} else
		ep->top = top;

	/*
	 * Figure out the new cursor line.  Increment by the suggested amount.
	 * If it's past the EOF, calculate a new one.
	 */
	lno = fm->lno + cnt;
	rp->lno = last < lno ? last : lno;
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
	recno_t off;

	off = vp->flags & VC_C1SET ? vp->count : 1;

	/* Can't go past the end of the file. */
	if (file_gline(ep, BOTLINE(ep, ep->otop) + off, NULL) == NULL) {
		v_eof(ep, fm);
		return (1);
	}

	/* Set the new top line. */
	ep->top += off;

	/*
	 * The cursor moves up, staying with its original line, unless it
	 * reaches the top of the screen.  If the line number changes,
         * we have to do screen relative movement (as if V_RCM was set),
         * otherwise, we set the relative movement (as if V_RCM_SET was set).
         * It's enough to make you cry.
	 */
	if (fm->lno <= ep->top) {
		rp->lno = ep->top;
		vp->kp->flags |= V_RCM;
	} else {
		rp->lno = fm->lno;
		vp->kp->flags |= V_RCM_SET;
	}
	return (0);
}
