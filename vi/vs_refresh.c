/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.52 1993/05/05 20:07:34 bostic Exp $ (Berkeley) $Date: 1993/05/05 20:07:34 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <curses.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"

static int	svi_modeline __P((SCR *, EXF *));
static int	svi_msgflush __P((SCR *));

/*
 * svi_refresh --
 *	This is the guts of the vi curses screen code.  The idea is that
 *	the SCR structure passed in contains the new coordinates of the
 *	screen.  What makes this hard is that we don't know how big
 *	characters are, doing input can put the cursor in illegal places,
 *	and we're frantically trying to avoid repainting unless it's
 *	absolutely necessary.  If you change this code, you'd better know
 *	what you're doing.  It's subtle and quick to anger.
 */
int
svi_refresh(sp, ep)
	SCR *sp;
	EXF *ep;
{
	CHNAME *cname;
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
		F_SET(sp, S_SSWITCH | S_REFORMAT | S_REDRAW);
		return (0);
	}

	/*
	 * 2: Reformat the lines.
	 *
	 * If the lines themselves have changed (:set list, for example),
	 * fill in the map from scratch.  Adjust the screen that's being
	 * displayed if the leftright flag is set.
	 */
	if (F_ISSET(sp, S_REFORMAT)) {
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
	 * HALFSCREEN(sp), we always ending up "filling" the map, with a
	 * P_MIDDLE flag, which isn't what the user wanted.  Tiny screens
	 * can go into the "fill" portions of the smap code, no problem.
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
	 * 3b: Line down.
	 */
	if (LNO >= HMAP->lno) {
		if (LNO <= TMAP->lno)
			goto adjust;

		/*
		 * If less than half a screen away, scroll down until the
		 * line is on the screen.
		 */
		lcnt = svi_sm_nlines(sp, ep, TMAP, LNO, HALFSCREEN(sp));
		if (lcnt < HALFSCREEN(sp)) {
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
		lastline = file_lline(sp, ep);
		tmp.lno = LNO;
		tmp.off = 1;
		lcnt = svi_sm_nlines(sp, ep, &tmp, lastline, sp->t_rows);
		if (lcnt < sp->t_rows) {
			if (svi_sm_fill(sp, ep, lastline, P_BOTTOM))
				return (1);
			goto paint;
		}

		/*
		 * If more than a full screen from the last line of the file,
		 * put the new line in the middle of the screen.
		 */
		goto middle;
	}

	/*
	 * 3c: Line up.
	 *
	 * If less than half a screen away, scroll up until the line is
	 * the first line on the screen.
	 */
	lcnt = svi_sm_nlines(sp, ep, HMAP, LNO, HALFSCREEN(sp));
	if (lcnt < HALFSCREEN(sp)) {
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
	lcnt = svi_sm_nlines(sp, ep, &tmp, LNO, HALFSCREEN(sp));
	if (lcnt < HALFSCREEN(sp))
		(void)svi_sm_fill(sp, ep, 1, P_TOP);
	else
middle:		(void)svi_sm_fill(sp, ep, LNO, P_MIDDLE);

	/*
	 * If we got here, we know the screen needs to be repainted, in
	 * which case, we can skip any cursor optimization.
	 */
	goto paint;

	/*
	 * At this point we know part of the line is on the screen.  Since
	 * scrolling is done using logical lines, not physical, all of the
	 * line may not be on the screen.  While that's not necessarily bad,
	 * if the part the cursor is on isn't there, we're going to lose.
	 * This isn't a problem for left-right scrolling, the cursor movement
	 * code will handle the problem.
	 */
adjust:	if (!O_ISSET(sp, O_LEFTRIGHT))
		if (LNO == HMAP->lno) {
			cnt = svi_screens(sp, ep, LNO, &CNO);
			while (cnt < HMAP->off)
				if (svi_sm_1down(sp, ep))
					return (1);
		} else if (LNO == TMAP->lno) {
			cnt = svi_screens(sp, ep, LNO, &CNO);
			while (cnt > TMAP->off)
				if (svi_sm_1up(sp, ep))
					return (1);
		}

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

	/*
	 * If a character is deleted from the line, we don't know how
	 * wide it was, so reparse.
	 */
	if (F_ISSET(sp, S_CUR_INVALID)) {
		F_CLR(sp, S_CUR_INVALID);
		goto slow;
	}

	/* If the line we're working with has changed, reparse. */
	if (LNO != OLNO)
		goto slow;

	/*
	 * Otherwise, if nothing's changed, go fast.  The one exception is
	 * that a single character or no characters are both column 0, and,
	 * if the single character required multiple screen columns, there
	 * may have still been movement.
	 */
	if (CNO == OCNO) {
		if (CNO == 0)
			goto slow;
		goto fast;
	}

	/*
	 * Get the current line.  If this fails, we either have an empty
	 * file and can just repaint, or there's a real problem.  This
	 * isn't a performance issue because there aren't any ways to get
	 * here repeatedly.
	 */
	if ((p = file_gline(sp, ep, LNO, &len)) == NULL) {
		if (file_lline(sp, ep) == 0)
			goto slow;
		GETLINE_ERR(sp, LNO);
		return (1);
	}

	/* This is just a test. */
#ifdef DEBUG
	if (CNO >= len) {
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
			if (O_ISSET(sp, O_LEFTRIGHT)) {
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
		 * character, go do it the the slow way.
		 */
		for (cwtotal = 0; cnt--; cwtotal += cname[ch].len)
			if ((ch = *(u_char *)p++) == '\t')
				goto slow;

		/*
		 * Increment the screen cursor by the total width of the
		 * characters.
		 */
		SCNO += cwtotal;

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
	y -= sp->s_off;			/* Correct for split screen. */
	goto update;

slow:	/* Find the current line in the map. */
	for (smp = HMAP; smp->lno != LNO; ++smp);

	/*
	 * If doing left-right scrolling, and the cursor movement has
	 * changed the screen being displayed, fix it.
	 */
	if (!ISINFOLINE(sp, smp) && O_ISSET(sp, O_LEFTRIGHT)) {
		cnt = svi_screens(sp, ep, LNO, &CNO) % SCREEN_COLS(sp);
		if (cnt != HMAP->off) {
			for (smp = HMAP; smp <= TMAP; ++smp)
				smp->off = cnt;
			goto paint;
		}
	}

	/* Update all of the screen lines for this file line. */
	for (; smp <= TMAP && smp->lno == LNO; ++smp)
		if (svi_line(sp, ep, smp, NULL, 0, &y, &SCNO))
			return (1);

	/* Not too bad, move the cursor. */
	goto update;

	/* Lost big, do what you have to do. */
paint:	for (smp = HMAP; smp <= TMAP; ++smp)
		if (svi_line(sp, ep, smp, NULL, 0, &y, &SCNO))
			return (1);
	F_CLR(sp, S_REDRAW);

update:	/* Ring the bell if scheduled. */
	if (F_ISSET(sp, S_BELLSCHED))
		svi_bell(sp);

	/*
	 * If the bottom line isn't in use by vi, display any
	 * messages or paint the mode line.
	 */
	if (!F_ISSET(&sp->bhdr, HDR_INUSE))
		if (sp->msgp != NULL && !F_ISSET(sp->msgp, M_EMPTY))
			svi_msgflush(sp);
		else if (!F_ISSET(sp, S_UPDATE_MODE)) {
			svi_modeline(sp, ep);
		}

	/* Place the cursor. */
	MOVE(sp, y, SCNO);

	/* Update saved information. */
	OCNO = CNO;
	OLNO = LNO;
	sp->sc_row = y;

	/* Refresh the screen. */
	if (F_ISSET(sp, S_REFRESH)) {
		wrefresh(curscr);
		F_CLR(sp, S_REFRESH);
	} else
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
	CHNAME *cname;
	MSG *mp;
	size_t chlen, len;
	int ch;
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

		/* Print up to the "more" message. */
		for (len = sp->cols - (sizeof(MCONTMSG) /* - 1 */);; ++p) {
			if (!mp->len)
				break;
			ch = *p;
			chlen = cname[ch].len;
			if (chlen >= len)
				break;
			len -= chlen;
			--mp->len;
			addnstr(cname[ch].name, chlen);
		}

		/* If more, print continue message. */
		if (mp->len ||
		    mp->next != NULL && !F_ISSET(mp->next, M_EMPTY)) {
			addnstr(MCONTMSG, sizeof(MCONTMSG) - 1);
			refresh();
			while (sp->special[ch = getkey(sp, 0)] != K_CR &&
			    !isspace(ch))
				svi_bell(sp);
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

#define	DIVIDESIZE	10
#define	RULERSIZE	15
#define	MODESIZE	(RULERSIZE + 15)

/*
 * svi_modeline --
 *	Update the mode line.
 */
int
svi_modeline(sp, ep)
	SCR *sp;
	EXF *ep;
{
	int dividesize;
	char buf[RULERSIZE];

	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();

	/* Display the dividing line. */
	if (sp->child != NULL) {
		dividesize = DIVIDESIZE > sp->cols ? sp->cols : DIVIDESIZE;
		memset(buf, ' ', dividesize);
		buf[dividesize] = '\0';
		standout();
		addstr(buf);
		standend();
	}

	/* Display the ruler and mode. */
	if (O_ISSET(sp, O_RULER) && sp->cols > RULERSIZE + 2) {
		MOVE(sp, INFOLINE(sp), sp->cols / 2 - RULERSIZE / 2);
		clrtoeol();
		(void)snprintf(buf,
		    sizeof(buf), "%lu,%lu", sp->lno, sp->cno + 1);
		addstr(buf);
	}

	/* Show the mode. */
	if (O_ISSET(sp, O_SHOWMODE) && sp->cols > MODESIZE) {
		MOVE(sp, INFOLINE(sp), sp->cols - 7);
		addstr(F_ISSET(sp, S_INPUT) ? "  Input" : "Command");
	}

	return (0);
}
