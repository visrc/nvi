/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_scroll.c,v 5.15 1992/12/05 11:10:41 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:10:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "screen.h"

#define	DOWN(lno) {							\
	if (file_gline(curf, lno, &len) == NULL) {			\
		v_eof(fm);						\
		return (1);						\
	}								\
	rp->lno = lno;							\
	rp->cno = len ? fm->cno > len - 1 ? len - 1 : fm->cno : 0;	\
}

/*
 * v_lgoto -- [count]G
 *	Go to first non-blank character of the line count, the last line
 *	of the file by default.
 */
int
v_lgoto(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last;

	last = file_lline(curf);
	if (vp->flags & VC_C1SET) {
		if (last < vp->count) {
			v_eof(fm);
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
v_home(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	lno = curf->otop + (vp->flags & VC_C1SET ? vp->count : 0);
	if (lno > file_lline(curf)) {
		v_eof(fm);
		return (1);
	}
	rp->lno = lno;
	return (0);
}

/*
 * v_middle -- M
 *	Move to the first non-blank character of the line in the middle
 *	of the screen.
 */
int
v_middle(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	if (file_gline(curf, BOTLINE(curf, curf->otop), NULL) == NULL) {
		lno = file_lline(curf) / 2;
		if (lno == 0)
			lno = 1;
		rp->lno = lno;
	} else
		rp->lno = curf->otop + curf->lines / 2;
	return (0);
}

/*
 * v_bottom -- [count]L
 *	Move to the first non-blank character of the line count from
 *	the bottom of the screen, 1 by default.
 */
int
v_bottom(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t cnt, lno;

	if (file_gline(curf, BOTLINE(curf, curf->otop), NULL) == NULL) {
		lno = file_lline(curf);
		if (lno == 0)
			lno = 1;
	} else
		lno = BOTLINE(curf, curf->otop);

	cnt = vp->flags & VC_C1SET ? vp->count : 0;
	if (cnt >= lno) {
		v_sof(fm);
		return (1);
	}
	rp->lno = lno - cnt;
	return (0);
}

/*
 * v_nbdown -- [count]^M, [count]+
 *	Move down by lines, moving cursor to first non-blank character.
 */
int
v_nbdown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;

	lno = fm->lno + (vp->flags & VC_C1SET ? vp->count : 1);

	DOWN(lno);
	return (0);
}

/*
 * v_down -- [count]^J, [count]^N, [count]j
 *	Move down by lines.
 */
int
v_down(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;

	lno = fm->lno + (vp->flags & VC_C1SET ? vp->count : 1);

	DOWN(lno);
	return (0);
}

/*
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(vp, fm, tm, rp)
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
	last = file_lline(curf);
	if (fm->lno == last) {
		v_eof(NULL);
		return (1);
	}

	/*
	 * Figure out the new top line.  Increment by the suggested amount.
	 * If it's past the EOF, calculate a new one, but don't make any
	 * change if the top line would be decremented (may be on a partial
	 * screen).
	 */
	top = curf->top + vp->count;
	if (last < BOTLINE(curf, top)) {
		if (last > SCREENSIZE(curf)) {
			top = last - SCREENSIZE(curf) + 1;
			if (top > curf->top)
				curf->top = top;
		}
	} else
		curf->top = top;

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
v_pagedown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t cnt, last, lno, top;

	/* Check for EOF. */
	last = file_lline(curf);
	if (fm->lno == last) {
		v_eof(NULL);
		return (1);
	}

	/*
	 * Figure out the new top line.  Increment by the suggested amount.
	 * If it's past the EOF, calculate a new one, but don't make any
	 * change if the top line would be decremented (may be on a partial
	 * screen).
	 * Calculation from POSIX 1003.2/D8.
	 */
	cnt = (vp->flags & VC_C1SET ? vp->count : 1) * (curf->lines - 3);
	top = curf->top + cnt;
	if (last < BOTLINE(curf, top)) {
		if (last > SCREENSIZE(curf)) {
			top = last - SCREENSIZE(curf) + 1;
			if (top > curf->top)
				curf->top = top;
		}
	} else
		curf->top = top;

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
v_linedown(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t off;

	off = vp->flags & VC_C1SET ? vp->count : 1;

	/* Can't go past the end of the file. */
	if (file_gline(curf, BOTLINE(curf, curf->otop) + off, NULL) == NULL) {
		v_eof(fm);
		return (1);
	}

	/* Set the new top line. */
	curf->top += off;

	/*
	 * The cursor moves up, staying with its original line, unless it
	 * reaches the top of the screen.  If the line number changes,
         * we have to do screen relative movement (as if V_RCM was set),
         * otherwise, we set the relative movement (as if V_RCM_SET was set).
         * It's enough to make you cry.
	 */
	if (fm->lno <= curf->top) {
		rp->lno = curf->top;
		vp->kp->flags |= V_RCM;
	} else {
		rp->lno = fm->lno;
		vp->kp->flags |= V_RCM_SET;
	}
	return (0);
}
