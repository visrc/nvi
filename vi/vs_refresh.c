/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 8.28 1993/11/13 18:01:18 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:01:18 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"
#include "sex/sex_screen.h"

static int	svi_modeline __P((SCR *, EXF *));
static int	svi_msgflush __P((SCR *));

int
svi_refresh(sp, ep)
	SCR *sp;
	EXF *ep;
{
	SCR *tsp;

#define	PAINTBITS	S_REDRAW | S_REFORMAT | S_REFRESH | S_RESIZE
	/* Paint any related screens that have changed. */
	for (tsp = sp->child; tsp != NULL; tsp = tsp->child)
		if (F_ISSET(tsp, PAINTBITS) ||
		    tsp->ep == ep && F_ISSET(SVP(tsp), SVI_SCREENDIRTY)) {
			(void)svi_paint(tsp, ep);
			F_CLR(SVP(tsp), SVI_SCREENDIRTY);
		}
	for (tsp = sp->parent; tsp != NULL; tsp = tsp->parent)
		if (F_ISSET(tsp, PAINTBITS) ||
		    tsp->ep == ep && F_ISSET(SVP(tsp), SVI_SCREENDIRTY)) {
			(void)svi_paint(tsp, ep);
			F_CLR(SVP(tsp), SVI_SCREENDIRTY);
		}
	/*
	 * Always refresh the current screen, it may be a cursor movement.
	 * Also, always do it last -- that way, S_REFRESH can be set in
	 * the current screen only, and the screen won't flash.
	 */
	F_CLR(sp, SVI_SCREENDIRTY);
	return (svi_paint(sp, ep));
}

/*
 * svi_paint --
 *	This is the guts of the vi curses screen code.  The idea is that
 *	the SCR structure passed in contains the new coordinates of the
 *	screen.  What makes this hard is that we don't know how big
 *	characters are, doing input can put the cursor in illegal places,
 *	and we're frantically trying to avoid repainting unless it's
 *	absolutely necessary.  If you change this code, you'd better know
 *	what you're doing.  It's subtle and quick to anger.
 */
int
svi_paint(sp, ep)
	SCR *sp;
	EXF *ep;
{
	CHNAME const *cname;
	SMAP *smp, tmp;
	recno_t lastline, lcnt;
	size_t cwtotal, cnt, len, x, y;
	int ch;
	char *p;

#define	 LNO	sp->lno
#define	OLNO	sp->olno
#define	 CNO	sp->cno
#define	OCNO	sp->ocno
#define	SCNO	sp->sc_col

	/*
	 * 1: Resize the window.
	 *
	 * Notice that a resize is requested, and set up everything so that
	 * the file gets reinitialized.  Done here, instead of in the vi
	 * loop because there may be other initialization that other screens
	 * need to do.  The actual changing of the row/column values was done
	 * by calling the ex options code which put them into the environment,
	 * which is used by curses.  Stupid, but ugly.
	 */
	if (F_ISSET(sp, S_RESIZE)) {
		/* Reinitialize curses. */
		if (svi_curses_end(sp) || svi_curses_init(sp))
			return (1);

		/* Toss svi_screens() cached information. */
		SVP(sp)->ss_lno = OOBLNO;

		/* Toss svi_line() cached information. */
		if (sp->s_fill(sp, ep, sp->lno, P_FILL))
			return (1);
		F_CLR(sp, S_RESIZE | S_REFORMAT);
		F_SET(sp, S_REDRAW);
	}

	/*
	 * 2: Reformat the lines.
	 *
	 * If the lines themselves have changed (:set list, for example),
	 * fill in the map from scratch.  Adjust the screen that's being
	 * displayed if the leftright flag is set.
	 */
	if (F_ISSET(sp, S_REFORMAT)) {
		/* Toss svi_screens() cached information. */
		SVP(sp)->ss_lno = OOBLNO;

		/* Toss svi_line() cached information. */
		if (svi_sm_fill(sp, ep, HMAP->lno, P_TOP))
			return (1);
		if (O_ISSET(sp, O_LEFTRIGHT) &&
		    (cnt = svi_screens(sp, ep, LNO, &CNO)) != 1)
			for (smp = HMAP; smp <= TMAP; ++smp)
				smp->off = cnt;
		F_CLR(sp, S_REFORMAT);
		F_SET(sp, S_REDRAW);
	}

	/*
	 * 3: Line movement.
	 *
	 * Line changes can cause the top line to change as well.  As
	 * before, if the movement is large, the screen is repainted.
	 *
	 * 3a: Tiny screens.
	 *
	 * Tiny screens cannot be permitted into the "scrolling" parts of
	 * the smap code for two reasons.  If the screen size is 1 line,
	 * HMAP == TMAP and the code will quickly drop core.  If the screen
	 * size is 2, none of the divisions by 2 will work, and scrolling
	 * won't work.  In fact, because no line change will be less than
	 * HALFTEXT(sp), we always ending up "filling" the map, with a
	 * P_MIDDLE flag, which isn't what the user wanted.  Tiny screens
	 * can go into the "fill" portions of the smap code, however.
	 */
	if (sp->t_rows <= 2) {
		if (LNO < HMAP->lno) {
			if (svi_sm_fill(sp, ep, LNO, P_TOP))
				return (1);
		} else if (LNO > TMAP->lno)
			if (svi_sm_fill(sp, ep, LNO, P_BOTTOM))
				return (1);
		if (sp->t_rows == 1) {
			HMAP->off = svi_screens(sp, ep, LNO, &CNO);
			goto paint;
		}
		F_SET(sp, S_REDRAW);
		goto adjust;
	}

	/*
	 * 3b: Small screens.
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
	 * was 1 line below the screen caused a window compress), and cursor
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
	 * code, when we should probably have compressed the window.
	 */
	if (ISSMALLSCREEN(sp))
		if (LNO < HMAP->lno) {
			lcnt = svi_sm_nlines(sp, ep, HMAP, LNO, sp->t_maxrows);
			if (lcnt <= HALFSCREEN(sp))
				for (; lcnt && sp->t_rows != sp->t_maxrows;
				     --lcnt, ++sp->t_rows) {
					++TMAP;
					if (svi_sm_1down(sp, ep))
						return (1);
				}
			else
				goto small_fill;
		} else if (LNO > TMAP->lno) {
			lcnt = svi_sm_nlines(sp, ep, TMAP, LNO, sp->t_maxrows);
			if (lcnt <= HALFSCREEN(sp))
				for (; lcnt && sp->t_rows != sp->t_maxrows;
				     --lcnt, ++sp->t_rows) {
					if (svi_sm_next(sp, ep, TMAP, TMAP + 1))
						return (1);
					++TMAP;
					if (svi_line(sp, ep, TMAP, NULL, NULL))
						return (1);
				}
			else {
small_fill:			MOVE(sp, INFOLINE(sp), 0);
				clrtoeol();
				for (; sp->t_rows > sp->t_minrows;
				    --sp->t_rows, --TMAP) {
					MOVE(sp, TMAP - HMAP, 0);
					clrtoeol();
				}
				if (svi_sm_fill(sp, ep, LNO, P_FILL))
					return (1);
				F_SET(sp, S_REDRAW);
				goto adjust;
			}
		}

	/*
	 * 3c: Line down.
	 */
	if (LNO >= HMAP->lno) {
		if (LNO <= TMAP->lno)
			goto adjust;

		/*
		 * If less than half a screen away, scroll down until the
		 * line is on the screen.
		 */
		lcnt = svi_sm_nlines(sp, ep, TMAP, LNO, HALFTEXT(sp));
		if (lcnt < HALFTEXT(sp)) {
			while (lcnt--)
				if (svi_sm_1up(sp, ep))
					return (1);
			goto adjust;
		}

		/*
		 * If less than a full screen from the bottom of the file, put
		 * the last line of the file on the bottom of the screen.  The
		 * calculation is safe because we know there's at least one
		 * full screen of lines, otherwise couldn't have gotten here.
		 */
		if (file_lline(sp, ep, &lastline))
			return (1);
		tmp.lno = LNO;
		tmp.off = 1;
		lcnt = svi_sm_nlines(sp, ep, &tmp, lastline, sp->t_rows);
		if (lcnt < sp->t_rows) {
			if (svi_sm_fill(sp, ep, lastline, P_BOTTOM))
				return (1);
			F_SET(sp, S_REDRAW);
			goto adjust;
		}

		/*
		 * If more than a full screen from the last line of the file,
		 * put the new line in the middle of the screen.
		 */
		goto middle;
	}

	/*
	 * 3d: Line up.
	 *
	 * If less than half a screen away, scroll up until the line is
	 * the first line on the screen.
	 */
	lcnt = svi_sm_nlines(sp, ep, HMAP, LNO, HALFTEXT(sp));
	if (lcnt < HALFTEXT(sp)) {
		while (lcnt--)
			if (svi_sm_1down(sp, ep))
				return (1);
		goto adjust;
	}

	/*
	 * If less than half a screen from the top of the file, put the first
	 * line of the file at the top of the screen.  Otherwise, put the line
	 * in the middle of the screen.
	 */
	tmp.lno = 1;
	tmp.off = 1;
	lcnt = svi_sm_nlines(sp, ep, &tmp, LNO, HALFTEXT(sp));
	if (lcnt < HALFTEXT(sp)) {
		if (svi_sm_fill(sp, ep, 1, P_TOP))
			return (1);
	} else
middle:		if (svi_sm_fill(sp, ep, LNO, P_MIDDLE))
			return (1);
	F_SET(sp, S_REDRAW);

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
		cnt = svi_screens(sp, ep, LNO, &CNO);
		if (LNO == HMAP->lno && cnt < HMAP->off)
			if ((HMAP->off - cnt) > HALFTEXT(sp)) {
				HMAP->off = cnt;
				svi_sm_fill(sp, ep, OOBLNO, P_TOP);
				F_SET(sp, S_REDRAW);
			} else
				while (cnt < HMAP->off)
					if (svi_sm_1down(sp, ep))
						return (1);
		if (LNO == TMAP->lno && cnt > TMAP->off)
			if ((cnt - TMAP->off) > HALFTEXT(sp)) {
				TMAP->off = cnt;
				svi_sm_fill(sp, ep, OOBLNO, P_BOTTOM);
				F_SET(sp, S_REDRAW);
			} else
				while (cnt > TMAP->off)
					if (svi_sm_1up(sp, ep))
						return (1);
	}

	/* If the screen needs to be repainted, skip cursor optimization. */
	if (F_ISSET(sp, S_REDRAW))
		goto paint;

	/*
	 * 4: Cursor movements.
	 *
	 * Decide cursor position.  If the line has changed, the cursor has
	 * moved over a tab, or don't know where the cursor was, reparse the
	 * line.  Note, if we think that the cursor "hasn't moved", reparse
	 * the line.  This is 'cause if it hasn't moved, we've almost always
	 * lost track of it.
	 *
	 * Otherwise, we've just moved over fixed-width characters, and can
	 * calculate the left/right scrolling and cursor movement without
	 * reparsing the line.  Note that we don't know which (if any) of
	 * the characters between the old and new cursor positions changed.
	 *
	 * XXX
	 * With some work, it should be possible to handle tabs quickly, at
	 * least in obvious situations, like moving right and encountering
	 * a tab, without reparsing the whole line.
	 */

	/* If the line we're working with has changed, reparse. */
	if (F_ISSET(SVP(sp), SVI_CUR_INVALID) || LNO != OLNO) {
		F_CLR(SVP(sp), SVI_CUR_INVALID);
		goto slow;
	}

	/* Otherwise, if nothing's changed, go fast. */
	if (CNO == OCNO)
		goto fast;

	/*
	 * Get the current line.  If this fails, we either have an empty
	 * file and can just repaint, or there's a real problem.  This
	 * isn't a performance issue because there aren't any ways to get
	 * here repeatedly.
	 */
	if ((p = file_gline(sp, ep, LNO, &len)) == NULL) {
		if (file_lline(sp, ep, &lastline))
			return (1);
		if (lastline == 0)
			goto slow;
		GETLINE_ERR(sp, LNO);
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
	cname = sp->cname;
	if (CNO < OCNO) {
		/*
		 * 4a: Cursor moved left.
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
		 * Quit sanity check -- it's hard to figure out exactly when
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
		for (cwtotal = 0; cnt--; cwtotal += cname[ch].len)
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
		if (cname[ch].len > 1)
			cwtotal -= cname[ch].len - 1;

		/*
		 * If the new column moved us out of the current screen,
		 * calculate a new screen.
		 */
		if (SCNO < cwtotal) {
lscreen:		if (O_ISSET(sp, O_LEFTRIGHT)) {
				for (smp = HMAP; smp <= TMAP; ++smp)
					--smp->off;
				goto paint;
			}
			goto slow;
		}
		SCNO -= cwtotal;
	} else {
		/*
		 * 4b: Cursor moved right.
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
			if ((cwtotal += cname[ch].len) >= SCREEN_COLS(sp))
				break;
		}

		/*
		 * Increment the screen cursor by the total width of the
		 * characters.
		 */
		SCNO = cwtotal;

		/*
		 * If the new column moved us out of the current screen,
		 * calculate a new screen.
		 */
		if (SCNO >= SCREEN_COLS(sp)) {
			if (O_ISSET(sp, O_LEFTRIGHT)) {
				SCNO -= SCREEN_COLS(sp);
				for (smp = HMAP; smp <= TMAP; ++smp)
					++smp->off;
				goto paint;
			}
			goto slow;
		}
	}

fast:	getyx(stdscr, y, x);		/* Just move the cursor. */
	y -= sp->s_off;			/* Correct for split screens. */
	goto update;

slow:	/* Find the current line in the map. */
	for (smp = HMAP; smp->lno != LNO; ++smp);

	/*
	 * If doing left-right scrolling and the cursor movement has changed
	 * the screen being displayed, scroll it.  If we're painting the info
	 * line, however, just scroll that single line.
	 */
	if (O_ISSET(sp, O_LEFTRIGHT)) {
		cnt = svi_screens(sp, ep, LNO, &CNO) % SCREEN_COLS(sp);
		if (cnt != HMAP->off) {
			if (ISINFOLINE(sp, smp))
				smp->off = cnt;
			else
				for (smp = HMAP; smp <= TMAP; ++smp)
					smp->off = cnt;
			goto paint;
		}
	}

	/*
	 * Update screen lines for this file line until we have a new
	 * screen cursor position.
	 */
	for (y = -1; smp <= TMAP && smp->lno == LNO; ++smp) {
		if (svi_line(sp, ep, smp, &y, &SCNO))
			return (1);
		if (y != -1)
			break;
	}

	/* Not too bad, move the cursor. */
	goto update;

	/*
	 * Lost big, do what you have to do.  We flush the cache since
	 * S_REDRAW gets set when the screen isn't worth fixing, and
	 * it's simpler to just repaint.  So, don't trust anything that
	 * we think we know about it.
	 */
paint:	for (smp = HMAP; smp <= TMAP; ++smp)
		SMAP_FLUSH(smp);
	for (smp = HMAP; smp <= TMAP; ++smp)
		if (svi_line(sp, ep, smp, &y, &SCNO))
			return (1);

	/*
	 * If it's a small screen and we're redrawing, clear the unused
	 * lines, ex may have overwritten them.
	 */
	if (F_ISSET(sp, S_REDRAW)) {
		if (ISSMALLSCREEN(sp))
			for (cnt = sp->t_rows; cnt <= sp->t_maxrows; ++cnt) {
				MOVE(sp, cnt, 0);
				clrtoeol();
			}
		F_CLR(sp, S_REDRAW);
	}

	/* Update saved information. */
update:	OCNO = CNO;
	OLNO = LNO;

	/* Refresh the screen. */
	if (F_ISSET(sp, S_REFRESH)) {
		wrefresh(curscr);
		F_CLR(sp, S_REFRESH);
	}

	/*
	 * If there are any keys waiting, we don't ring the bell or update
	 * the messages.  The reason is that there may be multiple messages,
	 * and we're going to get the wrong key as a message delimiter.
	 *
	 * XXX
	 * This is wrong.  If you type quickly enough, the status message
	 * for a screen may not be displayed during a split command because
	 * there were keys waiting so we switched screens without ever
	 * displaying it.  Maybe the split code should flush messages?
	 */
	if (!sex_key_wait(sp)) {
		if (F_ISSET(sp, S_BELLSCHED))
			svi_bell(sp);
		/*
		 * If the bottom line isn't in use by the colon command:
		 *
		 *	Display any messages.  Don't test S_UPDATE_MODE.  The
		 *	message printing routine set it to avoid anyone else
		 *	destroying the message we're about to display.
		 *
		 *	If the bottom line isn't in use by anyone, put out the
		 *	standard status line.
		 */
		if (!F_ISSET(SVP(sp), SVI_INFOLINE))
			if (sp->msgp != NULL && !F_ISSET(sp->msgp, M_EMPTY))
				svi_msgflush(sp);
			else if (!F_ISSET(sp, S_UPDATE_MODE))
				svi_modeline(sp, ep);
	}

	/* Place the cursor. */
	MOVE(sp, y, SCNO);

	/* Flush it all out. */
	refresh();

	return (0);
}

/*
 * svi_msgflush --
 *	Flush any accumulated messages.
 */
static int
svi_msgflush(sp)
	SCR *sp;
{
	CHAR_T ch;
	CHNAME const *cname;
	MSG *mp;
	size_t chlen, len;
	char *p;

#define	MCONTMSG	" [More ...]"

	/* Display the messages. */
	cname = sp->cname;
	for (mp = sp->msgp, p = NULL;
	    mp != NULL && !F_ISSET(mp, M_EMPTY); mp = mp->next) {

		p = mp->mbuf;

lcont:		/* Move to the message line and clear it. */
		MOVE(sp, INFOLINE(sp), 0);
		clrtoeol();

		/*
		 * Turn on standout mode if requested, or, if we've split
		 * the screen and need a divider.
		 */
		if (F_ISSET(mp, M_INV_VIDEO) || sp->child != NULL)
			standout();

		/*
		 * Print up to the "more" message.  Avoid the last character
		 * in the last line, some hardware doesn't like it.
		 */
		if (svi_ncols(sp, p, mp->len, NULL) < sp->cols - 1)
			len = sp->cols - 1;
		else
			len = (sp->cols - sizeof(MCONTMSG)) - 1;
		for (;; ++p) {
			if (!mp->len)
				break;
			ch = *(u_char *)p;
			chlen = cname[ch].len;
			if (chlen >= len)
				break;
			len -= chlen;
			--mp->len;
			ADDNSTR(cname[ch].name, chlen);
		}

		/*
		 * If more, print continue message.  If user key fails,
		 * keep showing the messages anyway.
		 */
		if (mp->len ||
		    (mp->next != NULL && !F_ISSET(mp->next, M_EMPTY))) {
			ADDNSTR(MCONTMSG, sizeof(MCONTMSG) - 1);
			refresh();
			for (;;) {
				if (term_key(sp, &ch, 0) != INP_OK)
					break;
				if (sp->special[ch] == K_CR ||
				    sp->special[ch] == K_NL || isblank(ch))
					break;
				svi_bell(sp);
			}
		}

		/* Turn off standout mode. */
		if (F_ISSET(mp, M_INV_VIDEO) || sp->child != NULL)
			standend();

		if (mp->len)
			goto lcont;

		refresh();
		F_SET(mp, M_EMPTY);
	}
	return (0);
}

#define	RULERSIZE	15
#define	MODESIZE	(RULERSIZE + 15)

/*
 * svi_modeline --
 *	Update the mode line.
 */
static int
svi_modeline(sp, ep)
	SCR *sp;
	EXF *ep;
{
	char *s, buf[RULERSIZE];

	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();

	/* Display the dividing line. */
	if (sp->child != NULL)
		svi_divider(sp);

	/* Display the ruler and mode. */
	if (O_ISSET(sp, O_RULER) && sp->cols > RULERSIZE + 2) {
		MOVE(sp, INFOLINE(sp), sp->cols / 2 - RULERSIZE / 2);
		clrtoeol();
		(void)snprintf(buf,
		    sizeof(buf), "%lu,%lu", sp->lno, sp->cno + 1);
		ADDSTR(buf);
	}

	/*
	 * Show the mode.  Leave the last character blank, in case it's a
	 * really dumb terminal with hardware scroll.  Second, don't try
	 * to *paint* the last character, SunOS 4.1.1 and Ultrix 4.2 curses
	 * won't let you paint the last character in the screen.
	 */
	if (O_ISSET(sp, O_SHOWMODE) && sp->cols > MODESIZE) {
		MOVE(sp, INFOLINE(sp), sp->cols - 8);
		s = F_ISSET(sp, S_INPUT) ? "  Input" : "Command";
		ADDSTR(s);
	}

	return (0);
}

int
svi_divider(sp)
	SCR *sp;
{
#define	DIVIDESIZE	10
	int dividesize;
	char buf[DIVIDESIZE + 1];

	dividesize = DIVIDESIZE > sp->cols ? sp->cols : DIVIDESIZE;
	memset(buf, ' ', dividesize);
	if (standout() == ERR)
		return (1);
	ADDNSTR(buf, dividesize);
	if (standend() == ERR)
		return (1);
	return (0);
}
