/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 10.8 1995/09/21 10:59:38 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:59:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

#define	PAINT_CURSOR	0x01			/* Update cursor. */
#define	PAINT_FLUSH	0x02			/* Flush to screen. */
static int	vs_paint __P((SCR *, u_int));

/*
 * v_repaint --
 *	Repaint selected lines from the screen.
 *
 * PUBLIC: int vs_repaint __P((SCR *, EVENT *));
 */
int
vs_repaint(sp, evp)
	SCR *sp;
	EVENT *evp;
{
	SMAP *smp;

	for (; evp->e_flno <= evp->e_tlno; ++evp->e_flno) {
		smp = HMAP + evp->e_flno - 1;
		SMAP_FLUSH(smp);
		if (vs_line(sp, smp, NULL, NULL))
			return (1);
	}
	return (0);
}

/*
 * vs_refresh --
 *	Refresh all screens.
 *
 * PUBLIC: int vs_refresh __P((SCR *));
 */
int
vs_refresh(sp)
	SCR *sp;
{
	SCR *tsp;
	u_int priv_paint, pub_paint;

	/*
	 * 1: Refresh the screen.
	 *
	 * If S_SCR_REDRAW is set in the current screen, repaint everything
	 * that we can find.
	 */
	if (F_ISSET(sp, S_SCR_REDRAW))
		for (tsp = sp->gp->dq.cqh_first;
		    tsp != (void *)&sp->gp->dq; tsp = tsp->q.cqe_next)
			if (tsp != sp)
				F_SET(tsp, S_SCR_REDRAW);

	/*
	 * 2: Related or dirtied screens, or screens with messages.
	 *
	 * If related screens share a view into a file, they may have been
	 * modified as well.  Refresh any screens that aren't exiting that
	 * have paint or dirty bits set.  Finally, if we refresh any screens
	 * other than the current one, the cursor will be trashed.
	 */
	pub_paint = S_SCR_REFORMAT | S_SCR_REDRAW;
	priv_paint = VIP_SCR_DIRTY;
	if (O_ISSET(sp, O_NUMBER))
		priv_paint |= VIP_SCR_NUMBER;
	for (tsp = sp->gp->dq.cqh_first;
	    tsp != (void *)&sp->gp->dq; tsp = tsp->q.cqe_next)
		if (tsp != sp && !F_ISSET(tsp, S_EXIT | S_EXIT_FORCE) &&
		    (F_ISSET(tsp, pub_paint) ||
		    F_ISSET(VIP(tsp), priv_paint))) {
			(void)vs_paint(tsp, 0);
			F_CLR(VIP(tsp), VIP_SCR_DIRTY);
			F_SET(VIP(sp), VIP_CUR_INVALID);
		}

	/*
	 * 3: Refresh the current screen.
	 *
	 * Always refresh the current screen, it may be a cursor movement.
	 * Also, always do it last -- that way, S_SCR_REDRAW can be set
	 * in the current screen only, and the screen won't flash.
	 */
	F_CLR(VIP(sp), VIP_SCR_DIRTY);
	if (vs_paint(sp, PAINT_CURSOR | PAINT_FLUSH))
		return (1);

	/*
	 * A side-effect of refreshing the screen is that we can now display
	 * messages in it.
	 */
	F_SET(sp, S_SCREEN_READY);
	return (0);
}

/*
 * vs_paint --
 *	This is the guts of the vi curses screen code.  The idea is that
 *	the SCR structure passed in contains the new coordinates of the
 *	screen.  What makes this hard is that we don't know how big
 *	characters are, doing input can put the cursor in illegal places,
 *	and we're frantically trying to avoid repainting unless it's
 *	absolutely necessary.  If you change this code, you'd better know
 *	what you're doing.  It's subtle and quick to anger.
 */
static int
vs_paint(sp, flags)
	SCR *sp;
	u_int flags;
{
	GS *gp;
	SMAP *smp, tmp;
	VI_PRIVATE *vip;
	recno_t lastline, lcnt;
	size_t cwtotal, cnt, len, x, y;
	int ch, didpaint, leftright_warp;
	char *p;

#define	 LNO	sp->lno
#define	OLNO	vip->olno
#define	 CNO	sp->cno
#define	OCNO	vip->ocno
#define	SCNO	vip->sc_col

	gp = sp->gp;
	vip = VIP(sp);
	didpaint = leftright_warp = 0;

	/*
	 * 4: Reformat the lines.
	 *
	 * If the lines themselves have changed (:set list, for example),
	 * fill in the map from scratch.  Adjust the screen that's being
	 * displayed if the leftright flag is set.
	 */
	if (F_ISSET(sp, S_SCR_REFORMAT)) {
		/* Invalidate the line size cache. */
		VI_SCR_CFLUSH(vip);

		/* Toss vs_line() cached information. */
		if (F_ISSET(sp, S_SCR_TOP)) {
			if (vs_sm_fill(sp, LNO, P_TOP))
				return (1);
		}
		else if (F_ISSET(sp, S_SCR_CENTER)) {
			if (vs_sm_fill(sp, LNO, P_MIDDLE))
				return (1);
		} else
			if (vs_sm_fill(sp, HMAP->lno, P_TOP))
				return (1);
		if (O_ISSET(sp, O_LEFTRIGHT) &&
		    (cnt = vs_opt_screens(sp, LNO, &CNO)) != 1)
			for (smp = HMAP; smp <= TMAP; ++smp)
				smp->off = cnt;
		F_SET(sp, S_SCR_REDRAW);
	}

	/*
	 * 5: Line movement.
	 *
	 * Line changes can cause the top line to change as well.  As
	 * before, if the movement is large, the screen is repainted.
	 *
	 * 5a: Small screens.
	 *
	 * Users can use the window, w300, w1200 and w9600 options to make
	 * the screen artificially small.  The behavior of these options
	 * in the historic vi wasn't all that consistent, and, in fact, it
	 * was never documented how various screen movements affected the
	 * screen size.  Generally, one of three things would happen:
	 *	1: The screen would expand in size, showing the line
	 *	2: The screen would scroll, showing the line
	 *	3: The screen would compress to its smallest size and
	 *		repaint.
	 * In general, scrolling didn't cause compression (200^D was handled
	 * the same as ^D), movement to a specific line would (:N where N
	 * was 1 line below the screen caused a screen compress), and cursor
	 * movement would scroll if it was 11 lines or less, and compress if
	 * it was more than 11 lines.  (And, no, I have no idea where the 11
	 * comes from.)
	 *
	 * What we do is try and figure out if the line is less than half of
	 * a full screen away.  If it is, we expand the screen if there's
	 * room, and then scroll as necessary.  The alternative is to compress
	 * and repaint.
	 *
	 * !!!
	 * This code is a special case from beginning to end.  Unfortunately,
	 * home modems are still slow enough that it's worth having.
	 *
	 * XXX
	 * If the line a really long one, i.e. part of the line is on the
	 * screen but the column offset is not, we'll end up in the adjust
	 * code, when we should probably have compressed the screen.
	 */
	if (IS_SMALL(sp))
		if (LNO < HMAP->lno) {
			lcnt = vs_sm_nlines(sp, HMAP, LNO, sp->t_maxrows);
			if (lcnt <= HALFSCREEN(sp))
				for (; lcnt && sp->t_rows != sp->t_maxrows;
				     --lcnt, ++sp->t_rows) {
					++TMAP;
					if (vs_sm_1down(sp))
						return (1);
				}
			else
				goto small_fill;
		} else if (LNO > TMAP->lno) {
			lcnt = vs_sm_nlines(sp, TMAP, LNO, sp->t_maxrows);
			if (lcnt <= HALFSCREEN(sp))
				for (; lcnt && sp->t_rows != sp->t_maxrows;
				     --lcnt, ++sp->t_rows) {
					if (vs_sm_next(sp, TMAP, TMAP + 1))
						return (1);
					++TMAP;
					if (vs_line(sp, TMAP, NULL, NULL))
						return (1);
				}
			else {
small_fill:			(void)gp->scr_move(sp, INFOLINE(sp), 0);
				(void)gp->scr_clrtoeol(sp);
				for (; sp->t_rows > sp->t_minrows;
				    --sp->t_rows, --TMAP) {
					(void)gp->scr_move(sp, TMAP - HMAP, 0);
					(void)gp->scr_clrtoeol(sp);
				}
				if (vs_sm_fill(sp, LNO, P_FILL))
					return (1);
				F_SET(sp, S_SCR_REDRAW);
				goto adjust;
			}
		}

	/*
	 * 5b: Line down, or current screen.
	 */
	if (LNO >= HMAP->lno) {
		/* Current screen. */
		if (LNO <= TMAP->lno)
			goto adjust;
		if (F_ISSET(sp, S_SCR_TOP))
			goto top;
		if (F_ISSET(sp, S_SCR_CENTER))
			goto middle;

		/*
		 * If less than half a screen above the line, scroll down
		 * until the line is on the screen.
		 */
		lcnt = vs_sm_nlines(sp, TMAP, LNO, HALFTEXT(sp));
		if (lcnt < HALFTEXT(sp)) {
			while (lcnt--)
				if (vs_sm_1up(sp))
					return (1);
			goto adjust;
		}
		goto bottom;
	}

	/*
	 * 5c: If not on the current screen, may request center or top.
	 */
	if (F_ISSET(sp, S_SCR_TOP))
		goto top;
	if (F_ISSET(sp, S_SCR_CENTER))
		goto middle;

	/*
	 * 5d: Line up.
	 */
	lcnt = vs_sm_nlines(sp, HMAP, LNO, HALFTEXT(sp));
	if (lcnt < HALFTEXT(sp)) {
		/*
		 * If less than half a screen below the line, scroll up until
		 * the line is the first line on the screen.  Special check so
		 * that if the screen has been emptied, we refill it.
		 */
		if (file_eline(sp, HMAP->lno)) {
			while (lcnt--)
				if (vs_sm_1down(sp))
					return (1);
			goto adjust;
		}

		/*
		 * If less than a half screen from the bottom of the file,
		 * put the last line of the file on the bottom of the screen.
		 */
bottom:		if (file_lline(sp, &lastline))
			return (1);
		tmp.lno = LNO;
		tmp.off = 1;
		lcnt = vs_sm_nlines(sp, &tmp, lastline, sp->t_rows);
		if (lcnt < HALFTEXT(sp)) {
			if (vs_sm_fill(sp, lastline, P_BOTTOM))
				return (1);
			F_SET(sp, S_SCR_REDRAW);
			goto adjust;
		}
		/* It's not close, just put the line in the middle. */
		goto middle;
	}

	/*
	 * If less than half a screen from the top of the file, put the first
	 * line of the file at the top of the screen.  Otherwise, put the line
	 * in the middle of the screen.
	 */
	tmp.lno = 1;
	tmp.off = 1;
	lcnt = vs_sm_nlines(sp, &tmp, LNO, HALFTEXT(sp));
	if (lcnt < HALFTEXT(sp)) {
		if (vs_sm_fill(sp, 1, P_TOP))
			return (1);
	} else
middle:		if (vs_sm_fill(sp, LNO, P_MIDDLE))
			return (1);
	if (0) {
top:		if (vs_sm_fill(sp, LNO, P_TOP))
			return (1);
	}
	F_SET(sp, S_SCR_REDRAW);

	/*
	 * At this point we know part of the line is on the screen.  Since
	 * scrolling is done using logical lines, not physical, all of the
	 * line may not be on the screen.  While that's not necessarily bad,
	 * if the part the cursor is on isn't there, we're going to lose.
	 * This can be tricky; if the line covers the entire screen, lno
	 * may be the same as both ends of the map, that's why we test BOTH
	 * the top and the bottom of the map.  This isn't a problem for
	 * left-right scrolling, the cursor movement code handles the problem.
	 *
	 * There's a performance issue here if editing *really* long lines.
	 * This gets to the right spot by scrolling, and, in a binary, by
	 * scrolling hundreds of lines.  If the adjustment looks like it's
	 * going to be a serious problem, refill the screen and repaint.
	 */
adjust:	if (!O_ISSET(sp, O_LEFTRIGHT) &&
	    (LNO == HMAP->lno || LNO == TMAP->lno)) {
		cnt = vs_opt_screens(sp, LNO, &CNO);
		if (LNO == HMAP->lno && cnt < HMAP->off)
			if ((HMAP->off - cnt) > HALFTEXT(sp)) {
				HMAP->off = cnt;
				vs_sm_fill(sp, OOBLNO, P_TOP);
				F_SET(sp, S_SCR_REDRAW);
			} else
				while (cnt < HMAP->off)
					if (vs_sm_1down(sp))
						return (1);
		if (LNO == TMAP->lno && cnt > TMAP->off)
			if ((cnt - TMAP->off) > HALFTEXT(sp)) {
				TMAP->off = cnt;
				vs_sm_fill(sp, OOBLNO, P_BOTTOM);
				F_SET(sp, S_SCR_REDRAW);
			} else
				while (cnt > TMAP->off)
					if (vs_sm_1up(sp))
						return (1);
	}

	/*
	 * If the screen needs to be repainted, skip cursor optimization.
	 * However, in the code above we skipped leftright scrolling on
	 * the grounds that the cursor code would handle it.  Make sure
	 * the right screen is up.
	 */
	if (F_ISSET(sp, S_SCR_REDRAW)) {
		if (O_ISSET(sp, O_LEFTRIGHT)) {
			cnt = vs_opt_screens(sp, LNO, &CNO);
			if (HMAP->off != cnt)
				for (smp = HMAP; smp <= TMAP; ++smp)
					smp->off = cnt;
		}
		goto paint;
	}

	/*
	 * 6: Cursor movements (current screen only).
	 */
	if (!LF_ISSET(PAINT_CURSOR))
		goto number;

	/*
	 * Decide cursor position.  If the line has changed, the cursor has
	 * moved over a tab, or don't know where the cursor was, reparse the
	 * line.  Otherwise, we've just moved over fixed-width characters,
	 * and can calculate the left/right scrolling and cursor movement
	 * without reparsing the line.  Note that we don't know which (if any)
	 * of the characters between the old and new cursor positions changed.
	 *
	 * XXX
	 * With some work, it should be possible to handle tabs quickly, at
	 * least in obvious situations, like moving right and encountering
	 * a tab, without reparsing the whole line.
	 */

	/* If the line we're working with has changed, reparse. */
	if (F_ISSET(VIP(sp), VIP_CUR_INVALID) || LNO != OLNO)
		goto slow;

	/* Otherwise, if nothing's changed, go fast. */
	if (CNO == OCNO)
		goto fast;

	/*
	 * Get the current line.  If this fails, we either have an empty
	 * file and can just repaint, or there's a real problem.  This
	 * isn't a performance issue because there aren't any ways to get
	 * here repeatedly.
	 */
	if ((p = file_gline(sp, LNO, &len)) == NULL) {
		if (file_lline(sp, &lastline))
			return (1);
		if (lastline == 0)
			goto slow;
		FILE_LERR(sp, LNO);
		return (1);
	}

#ifdef DEBUG
	/* This is just a test. */
	if (CNO >= len && len != 0) {
		msgq(sp, M_ERR, "Error: %s/%d: cno (%u) >= len (%u)",
		     tail(__FILE__), __LINE__, CNO, len);
		return (1);
	}
#endif
	/*
	 * The basic scheme here is to look at the characters in between
	 * the old and new positions and decide how big they are on the
	 * screen, and therefore, how many screen positions to move.
	 */
	if (CNO < OCNO) {
		/*
		 * 6a: Cursor moved left.
		 *
		 * Point to the old character.  The old cursor position can
		 * be past EOL if, for example, we just deleted the rest of
		 * the line.  In this case, since we don't know the width of
		 * the characters we traversed, we have to do it slowly.
		 */
		p += OCNO;
		cnt = (OCNO - CNO) + 1;
		if (OCNO >= len)
			goto slow;

		/*
		 * Quick sanity check -- it's hard to figure out exactly when
		 * we cross a screen boundary as we do in the cursor right
		 * movement.  If cnt is so large that we're going to cross the
		 * boundary no matter what, stop now.
		 */
		if (SCNO + 1 + MAX_CHARACTER_COLUMNS < cnt)
			goto lscreen;

		/*
		 * Count up the widths of the characters.  If it's a tab
		 * character, go do it the the slow way.
		 */
		for (cwtotal = 0; cnt--; cwtotal += KEY_LEN(sp, ch))
			if ((ch = *(u_char *)p--) == '\t')
				goto slow;

		/*
		 * Decrement the screen cursor by the total width of the
		 * characters minus 1.
		 */
		cwtotal -= 1;

		/*
		 * If we're moving left, and there's a wide character in the
		 * current position, go to the end of the character.
		 */
		if (KEY_LEN(sp, ch) > 1)
			cwtotal -= KEY_LEN(sp, ch) - 1;

		/*
		 * If the new column moved us off of the current logical line,
		 * calculate a new one.  If doing leftright scrolling, we've
		 * moved off of the current screen, as well.  Since most files
		 * don't have more than two screens, we optimize moving from
		 * screen 2 to screen 1.
		 */
		if (SCNO < cwtotal) {
lscreen:		if (O_ISSET(sp, O_LEFTRIGHT)) {
				cnt = HMAP->off == 2 ? 1 :
				    vs_opt_screens(sp, LNO, &CNO);
				for (smp = HMAP; smp <= TMAP; ++smp)
					smp->off = cnt;
				leftright_warp = 1;
				goto paint;
			}
			goto slow;
		}
		SCNO -= cwtotal;
	} else {
		/*
		 * 6b: Cursor moved right.
		 *
		 * Point to the first character to the right.
		 */
		p += OCNO + 1;
		cnt = CNO - OCNO;

		/*
		 * Count up the widths of the characters.  If it's a tab
		 * character, go do it the the slow way.  If we cross a
		 * screen boundary, we can quit.
		 */
		for (cwtotal = SCNO; cnt--;) {
			if ((ch = *(u_char *)p++) == '\t')
				goto slow;
			if ((cwtotal += KEY_LEN(sp, ch)) >= SCREEN_COLS(sp))
				break;
		}

		/*
		 * Increment the screen cursor by the total width of the
		 * characters.
		 */
		SCNO = cwtotal;

		/* See screen change comment in section 4a. */
		if (SCNO >= SCREEN_COLS(sp)) {
			if (O_ISSET(sp, O_LEFTRIGHT)) {
				cnt = vs_opt_screens(sp, LNO, &CNO);
				for (smp = HMAP; smp <= TMAP; ++smp)
					smp->off = cnt;
				leftright_warp = 1;
				goto paint;
			}
			goto slow;
		}
	}

	/*
	 * 6c: Fast cursor update.
	 *
	 * Retrieve the current cursor position, and correct it
	 * for split screens.
	 */
fast:	(void)gp->scr_cursor(sp, &y, &x);
	goto number;

	/*
	 * 6d: Slow cursor update.
	 *
	 * Walk through the map and find the current line.  If doing left-right
	 * scrolling and the cursor movement has changed the screen displayed,
	 * scroll the screen left or right, unless we're updating the info line
	 * in which case we just scroll that one line.  Then update the screen
	 * lines for this file line until we have a new screen cursor position.
	 */
slow:	for (smp = HMAP; smp->lno != LNO; ++smp);
	if (O_ISSET(sp, O_LEFTRIGHT)) {
		cnt = vs_opt_screens(sp, LNO, &CNO) % SCREEN_COLS(sp);
		if (cnt != HMAP->off) {
			if (F_ISSET(sp, S_INPUT_INFO))
				smp->off = cnt;
			else {
				for (smp = HMAP; smp <= TMAP; ++smp)
					smp->off = cnt;
				leftright_warp = 1;
			}
			goto paint;
		}
	}
	for (y = -1; smp <= TMAP && smp->lno == LNO; ++smp) {
		if (vs_line(sp, smp, &y, &SCNO))
			return (1);
		if (y != -1) {
			vip->sc_smap = smp;
			break;
		}
	}
	goto number;

	/*
	 * 7: Repaint the entire screen.
	 *
	 * Lost big, do what you have to do.  We flush the cache, since
	 * S_SCR_REDRAW gets set when the screen isn't worth fixing, and
	 * it's simpler to repaint.  So, don't trust anything that we
	 * think we know about it.
	 */
paint:	for (smp = HMAP; smp <= TMAP; ++smp)
		SMAP_FLUSH(smp);
	for (y = -1,
	    vip->sc_smap = NULL, smp = HMAP; smp <= TMAP; ++smp) {
		if (vs_line(sp, smp, &y, &SCNO))
			return (1);
		if (y != -1 && vip->sc_smap == NULL)
			vip->sc_smap = smp;
	}

	/*
	 * If it's a small screen and we're redrawing, clear the unused lines,
	 * ex may have overwritten them.
	 */
	if (F_ISSET(sp, S_SCR_REDRAW) && IS_SMALL(sp))
		for (cnt = sp->t_rows; cnt <= sp->t_maxrows; ++cnt) {
			(void)gp->scr_move(sp, cnt, 0);
			(void)gp->scr_clrtoeol(sp);
		}

	didpaint = 1;

	/*
	 * 8: Repaint the line numbers.
	 *
	 * If O_NUMBER is set and the VIP_SCR_NUMBER bit is set, and we
	 * didn't repaint the screen, repaint all of the line numbers,
	 * they've changed.
	 */
number:	if (O_ISSET(sp, O_NUMBER) &&
	    F_ISSET(vip, VIP_SCR_NUMBER) && !didpaint && vs_number(sp))
			return (1);

	/*
	 * 9: Flush changes to the screen.
	 *
	 * Update and position the cursor, and flush.
	 */
	if (LF_ISSET(PAINT_FLUSH)) {
		OCNO = CNO;
		OLNO = LNO;
		(void)gp->scr_move(sp, y, SCNO);
		(void)gp->scr_refresh(sp, 0);

		/*
		 * XXX
		 * Recalculate the "most favorite" cursor position.  Vi won't
		 * know that we've warped the screen, so it's going to have a
		 * wrong idea about where the cursor should be.  This is vi's
		 * problem, and fixing it here is a gross layering violation.
		 */
		if (leftright_warp)
			(void)vs_column(sp, &sp->rcm);
	}

	/* 10: Clear the flags that are handled by this routine. */
	F_CLR(sp, S_SCR_CENTER | S_SCR_REDRAW | S_SCR_REFORMAT | S_SCR_TOP);
	F_CLR(vip, VIP_CUR_INVALID | VIP_SCR_NUMBER);

	return (0);

#undef	 LNO
#undef	OLNO
#undef	 CNO
#undef	OCNO
#undef	SCNO
}
