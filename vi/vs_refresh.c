/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.30 1993/02/19 18:37:03 bostic Exp $ (Berkeley) $Date: 1993/02/19 18:37:03 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "options.h"
#include "screen.h"
#include "vcmd.h"

/*
 * This code is the screen interface to the vi editor.  It's commented
 * extremely heavily because it's downright nasty.  Changing this code
 * can be hazardous to your health.
 *
 * We use a single curses window for vi.  The model would be simpler with
 * two windows (one for the text, and one for the modeline) because
 * scrolling the text window down would work correctly then, not affecting
 * the mode line.  As it is we have to play games to make it look right.
 * The reason is that it would be difficult for curses to optimize the
 * movement, i.e. detect that the downward scroll isn't going to change
 * the modeline, set the scrolling region on the terminal and only scroll
 * the first part of the text window.  (Even if curses did detect it, the
 * set-scrolling-region terminal commands can't be used by curses because
 * it's indeterminate where the cursor ends up after they are sent.)
 */

#define	HALFSCREEN(ep)	(SCREENSIZE(ep) / 2)	/* Middle of the screen. */

/*
 * screen_init --
 *	Initialize curses and the screen.
 */
int
scr_begin(ep)
	EXF *ep;
{
	if (initscr() == NULL) {
		msg(ep, M_ERROR, "Error: initscr failed: %s", strerror(errno));
		return (1);
	}
	noecho();
	nonl();
	raw();
	scrollok(stdscr, 1);
	idlok(stdscr, 1);

	ep->lines = LINES;		/* XXX: Way ugly. */
	ep->cols = COLS;

	/* Force redraw. */
	FF_SET(ep, F_REDRAW);

	return (scr_sm_init(ep));
}

/*
 * scr_end --
 *	Move to the bottom of the screen, end curses.
 */
int
scr_end(ep)
	EXF *ep;
{
	if (move(SCREENSIZE(ep), 0) == OK) {
		clrtoeol();
		refresh();
	}
	endwin();			/* No delwin(), initscr() does them. */

	if (HMAP != NULL) {
		free(HMAP);
		ep->h_smap = NULL;	/* HMAP = NULL; */
	}
	return (0);
}

/*
 * scr_change --
 *	Notify the screen interface that a line, which may or may not
 *	be on the screen, has changed.
 * XXX
 *	I'm not sure that the lines which `invalidate the cursor position'
 *	are right.  It seems like they could be changed to figure out what
 *	the new cursor position has to be.
 */
int
scr_change(ep, lno, action)
	EXF *ep;
	recno_t lno;
	u_int action;
{
	size_t oldy, oldx;

	/* Appending is the same as inserting, if the line is incremented. */
	if (action & LINE_APPEND)
		++lno;

	/* Ignore the change if the line is not on the screen. */
	if (lno < HMAP->lno || lno > TMAP->lno)
		return (0);

	getyx(stdscr, oldy, oldx);

	switch(action & ~LINE_LOGICAL) {
	case LINE_DELETE:
		/* Update SMAP. */
		if (scr_sm_delete(ep, lno, action & LINE_LOGICAL))
			return (1);

		/*
		 * If not a physical deletion, i.e. a movement, and deleting
		 * the top line, then the top line and the line number must
		 * be incremented.
		 */
		if (action & LINE_LOGICAL && lno == ep->otop)
			++ep->otop;

		/* Invalidate the cursor. */
		ep->olno = OOBLNO;
		break;

	case LINE_APPEND:
	case LINE_INSERT:
		/* Update SMAP. */
		if (scr_sm_insert(ep, lno, action & LINE_LOGICAL))
			return (1);

		/*
		 * If not a physical insertion, i.e. a movement, and
		 * we're inserting at the top of the screen, then the
		 * top line and the line number must be incremented.
		 */
		if (action & LINE_LOGICAL && lno == ep->otop)
			--ep->otop;

		/* Invalidate the cursor. */
		ep->olno = OOBLNO;
		break;

	case LINE_RESET:
		/* Update SMAP. */
		if (scr_sm_delete(ep, lno, 0))
			return (1);
		if (scr_sm_insert(ep, lno, 0))
			return (1);
		break;
	default:
		abort();
	}

	MOVE(ep, oldy, oldx);

	return (0);
}

/*
 * scr_update --
 *	Change the screen as necessary for a cursor movement.  This is the
 *	guts of the screen code.  The idea is that the EXF structure passed
 *	in contains the new coordinates of the screen.  What makes this hard
 *	is that we don't know how big characters are, doing input can put
 *	the cursor in illegal places, and we're frantically trying to avoid
 *	repainting unless it's absolutely necessary.  If you change this code,
 *	you'd better know what you're doing.  It's subtle and quick to anger.
 */
int
scr_update(ep)
	EXF *ep;
{
	SMAP *sp;
	recno_t lastline, lcnt;
	size_t cwtotal, cnt, len, x, y;
	int ch;
	u_char *p;

	/*
	 * 1: Resize the window.
	 *
	 * The actual changing of the row/column values was done by calling
	 * the ex options code which entered them into the environment.  The
	 * function scr_end() uses the values from the EXF structure, and
	 * then scr_begin() uses the the new values, from the curses
	 * package, which got them from the environment.  We hope.
	 *
	 * Once the resize is done, we fall through.  The reason for this is
	 * that RESIZE or REDRAW might be set along with another movement.
	 * So, fall through until we figure out what the map should look like
	 * and then jump to repaint.
	 */
	if (FF_ISSET(ep, F_RESIZE)) {
		if (scr_end(ep) || scr_begin(ep))
			return (1);

		FF_CLR(ep, F_RESIZE);
		FF_SET(ep, F_REDRAW);
	}

	/*
	 * 2: Window (not line) movement.
	 *
	 * The EXF 'top' value is set if the window itself has moved, as
	 * opposed to the lines within the window moving.  If it's a big
	 * movement, repaint the screen.  Otherwise, scroll the screen as
	 * necessary to get the right top line in place.
	 */
	if (ep->top != ep->otop) {
		if (FF_ISSET(ep, F_REDRAW))		/* Redraw flag. */
			goto win_redraw;
							/* Check distance. */
		lcnt = scr_sm_nlines(ep, HMAP, ep->top, HALFSCREEN(ep));
		if (lcnt > HALFSCREEN(ep)) {
win_redraw:		(void)scr_sm_fill(ep, ep->top, P_TOP);
			goto paint;
		}

		if (ep->top > ep->otop) {		/* Window move down. */
			while (HMAP->lno != ep->top || HMAP->off != 1)
				if (scr_sm_1up(ep))
					return (1);
		} else					/* Window move up. */
			while (HMAP->lno != ep->top)
				if (scr_sm_1down(ep))
					return (1);
	}
		
	/*
	 * 3: Line movement.
	 *
	 * Line changes can cause the top line to change as well.  As
	 * before, if the movement is large, the screen is repainted.
	 *
	 * 3a: Line down.
	 */
	if (ep->lno > ep->otop) {
		/* If on the current screen, we're done. */
		if (ep->lno <= TMAP->lno) {
			if (FF_ISSET(ep, F_REDRAW))
				goto paint;
			goto cursor;
		}

		/*
		 * If less than half a screen away, scroll down until the
		 * desired line is on the screen.
		 */
		lcnt = scr_sm_nlines(ep, TMAP, ep->lno, HALFSCREEN(ep));
		if (lcnt <= HALFSCREEN(ep)) {
			while (lcnt--)
				if (scr_sm_1up(ep))
					return (1);
			if (FF_ISSET(ep, F_REDRAW))
				goto paint;
			goto cursor;
		}

		/*
		 * If less than a full screen from the bottom of the file, put
		 * the last line of the file on the bottom of the screen.  The
		 * calculation is safe because we know there's at least one
		 * full screen of lines, otherwise couldn't have gotten here.
		 */
		lastline = file_lline(ep);
		HMAP->lno = ep->lno;
		HMAP->off = 1;
		lcnt = scr_sm_nlines(ep, HMAP, lastline, TEXTSIZE(ep));
		if (lcnt < TEXTSIZE(ep)) {
			scr_sm_fill(ep, lastline, P_BOTTOM);
			goto paint;
		}

		/*
		 * If more than a full screen from the last line of the file,
		 * put the new line in the middle of the screen.
		 */
		goto middle;
	}

	/*
	 * 3b: Line up.
	 *
	 * If less than half a screen away, reset the top line and scroll up
	 * until the desired line is the first line on the screen.
	 */
	lcnt = scr_sm_nlines(ep, HMAP, ep->lno, HALFSCREEN(ep));
	if (lcnt <= HALFSCREEN(ep)) {
		while (lcnt--)
			if (scr_sm_1down(ep))
				return (1);
		if (FF_ISSET(ep, F_REDRAW))
			goto paint;
		goto cursor;
	}

	/*
	 * If less than half a screen from the top of the file, put the first
	 * line of the file at the top of the screen.  Otherwise, put the line
	 * in the middle of the screen.
	 */
	lcnt = scr_sm_nlines(ep, HMAP, ep->lno, HALFSCREEN(ep));
	if (lcnt < HALFSCREEN(ep))
		(void)scr_sm_fill(ep, 1, P_TOP);
	else
middle:		(void)scr_sm_fill(ep, ep->lno, P_MIDDLE);
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
	 * reparsing the line.  Note that we don't know which (if any) of the
	 * characters between the old and new cursor positions changed.
	 *
	 * XXX
	 * With some work, it should be possible to handle tabs quickly, at
	 * least in obvious situations, like moving right and encountering
	 * a tab, without looking over the whole line.
	 */

cursor:	/* If line has changed. */
	if (ep->lno != ep->olno)
		goto slow;

	/*
	 * If a character is deleted from the line, we can't figure out
	 * how wide the character was so we have to do it slowly.
	 */
	if (FF_ISSET(ep, F_CHARDELETED)) {
		FF_CLR(ep, F_CHARDELETED);
		goto slow;
	}

	/*
	 * If no movement, don't trust it (we shouldn't be updating the
	 * screen if nothing happened).
	 */
	if (ep->cno == ep->ocno)
		goto slow;

	/*
	 * Get the current line.  If this fails, we either have an empty
	 * file and we should just repaint, or there's a real problem.
	 * This isn't a performance issue because there aren't any ways
	 * to get this to hit repeatedly.
	 */
	if ((p = file_gline(ep, ep->lno, &len)) == NULL) {
		if (file_lline(ep) == 0)
			goto paint;
		GETLINE_ERR(ep, ep->lno);
		return (0);
	}

	/*
	 * 4a: Cursor moved left.
	 *
	 * The basic scheme here is to look at the characters in between
	 * the old and new positions and decide how big they are on the
	 * screen, and therefore, how many screen positions to move.
	 */
	if (ep->cno < ep->ocno) {
		/*
		 * Point to the old character.  The old cursor position can
		 * be past EOL if, for example, we just deleted the rest of
		 * the line.  In this case, since we don't know the width of
		 * the characters we traversed, we have to do it slowly.
		 */
		p += ep->ocno;
		cnt = (ep->ocno - ep->cno) + 1;
		if (ep->ocno >= len)
			goto slow;

		/*
		 * Count up the widths of the characters.  If it's a tab
		 * character, just do it the the slow way.
		 */
		for (cwtotal = 0; cnt--; cwtotal += asciilen[ch])
			if ((ch = *p--) == '\t')
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
		if (asciilen[ch] > 1)
			cwtotal -= asciilen[ch] - 1;

		/*
		 * If the new column moved us out of the current screen,
		 * calculate a new screen.
		 */
		if (ep->scno < cwtotal) {
			if (ISSET(O_LEFTRIGHT)) {
				for (sp = HMAP; sp <= TMAP; ++sp)
					--sp->off;
				goto paint;
			}
			goto slow;
		}
		ep->scno -= cwtotal;
	} else {
		/*
		 * 4b: Cursor moved right.
		 *
		 * Point to the old character.
		 */
		p += ep->ocno;
		cnt = (ep->cno - ep->ocno) + 1;
#ifdef DEBUG
		if (ep->cno >= len) {
			msg(ep, M_ERROR,
			    "Error: %s/%d: ep->cno (%u) >= len (%u)",
			     tail(__FILE__), __LINE__, ep->cno, len);
			return (1);
		}
#endif
		/* Save the width of the original character. */
		len = asciilen[*p];

		/*
		 * Count up the widths of the characters.  If it's a tab
		 * character, just do it the the slow way.
		 */
		for (cwtotal = 0; cnt--; cwtotal += asciilen[ch])
			if ((ch = *p++) == '\t')
				goto slow;

		/*
		 * Increment the screen cursor by the total width of the
		 * characters minus the width of the original character.
		 */
		ep->scno += cwtotal - len;

		/*
		 * If the new column moved us out of the current screen,
		 * calculate a new screen.
		 */
		if (ep->scno >= SCREEN_COLS(ep)) {
			if (ISSET(O_LEFTRIGHT)) {
				ep->scno -= SCREEN_COLS(ep);
				for (sp = HMAP; sp <= TMAP; ++sp)
					++sp->off;
				goto paint;
			}
			goto slow;
		}
	}

	/* It was easy, just move the cursor. */
	getyx(stdscr, y, x);
	goto update;

slow:	/* Find the line. */
	for (sp = HMAP; sp->lno != ep->lno; ++sp);

	/*
	 * If doing left-right scrolling and the cursor movement has
	 * changed the screen being displayed, fix it.
	 */
	if (ISSET(O_LEFTRIGHT)) {
		cnt = scr_screens(ep, ep->lno, &ep->cno) % SCREEN_COLS(ep);
		if (cnt != HMAP->off) {
			for (sp = HMAP; sp <= TMAP; ++sp)
				sp->off = cnt;
			goto paint;
		}
	}

	/* Update all of the screen lines for this file line. */
	for (; sp <= TMAP && sp->lno == ep->lno; ++sp)
		if (scr_line(ep, sp, NULL, 0, &y, &ep->scno))
			return (1);

	/* Not too bad, just move the cursor. */
	goto update;

	/* Lost big, do what you have to do. */
paint:	for (sp = HMAP; sp <= TMAP; ++sp)
		if (scr_line(ep, sp, NULL, 0, &y, &ep->scno))
			return (1);
	FF_CLR(ep, F_REDRAW);
		
update:	MOVE(ep, y, ep->scno);

	/* If the screen has been trashed, refresh it all. */
	if (FF_ISSET(ep, F_REFRESH)) {
		wrefresh(curscr);
		FF_CLR(ep, F_REFRESH);
	}

	/*
	 * Update the screen information.
	 *
	 * XXX
	 * The whole usage of otop and top is probably wrong.  It would
	 * probably be simpler to encode screen routines that do the
	 * movement and return the physical cursor position.
	 */
	ep->ocno = ep->cno;
	ep->olno = ep->lno;
	ep->top = ep->otop = HMAP->lno;

	return (0);
}

/*
 * scr_line --
 *	Update one line on the screen.  One nasty little side effect is
 *	that it returns the screen position for the current character.
 *	Not clean, but this is the only routine that really knows what's
 *	out there.
 * XXX
 * 	Should cache offset into last line for last screen -- this should
 *	speed up folded lines a lot.  The problem is that if a tab is broken
 *	across the line, it's going to be tricky.  Also, are there really
 *	enough folded lines that this is worthwhile?
 */
int
scr_line(ep, sp, p, len, yp, xp)
	EXF *ep;
	SMAP *sp;
	register u_char *p;
	size_t len, *xp, *yp;
{
	size_t chlen, cols_per_screen, cno_cnt, count_cols;
	size_t offset_in_char, skip_cols;
	int ch;
	char nbuf[10];

	/* Move to the line. */
	MOVE(ep, sp - HMAP, 0);

	/*
	 * If O_NUMBER is set and this is the first screen of a folding
	 * line or any left-right line, display the line number.  Set
	 * the number of columns for this screen.
	 */
	if (ISSET(O_NUMBER) && (ISSET(O_LEFTRIGHT) || sp->off == 1)) {
		cols_per_screen = ep->cols -
		    snprintf(nbuf, sizeof(nbuf), O_NUMBER_FMT, sp->lno);
		addstr(nbuf);
	} else
		cols_per_screen = ep->cols;

	/*
	 * Get a copy of the line.  Special case non-existent lines and the
	 * first line of an empty file.  In both cases, the cursor position
	 * is 0.
	 */
	if (p == NULL)
		p = file_gline(ep, sp->lno, &len);
	if (p == NULL || len == 0) {
		if (yp != NULL && sp->lno == ep->lno) {
			*xp = 0;
			*yp = sp - HMAP;
		}
		if (sp->lno > file_lline(ep))
			addch(sp->lno == 1 ? ISSET(O_LIST) ? '$' : ' ' : '~');
		else if (p == NULL) {
			GETLINE_ERR(ep, sp->lno);
			return (1);
		}
		clrtoeol();
		return (0);
	}

	/*
	 * Set the number of column positions to skip until a character
	 * gets displayed.
	 */
	skip_cols = sp->off - 1;
	count_cols = 0;

	/*
	 * Set the number of characters to skip before reach the cursor
	 * character.  Offset by 1 and use 0 as a flag value.  We may be
	 * called repeatedly with a valid pointer to a cursor position.
	 * Don't fill it in unless it's the right line.
	 */
	cno_cnt = yp == NULL || sp->lno != ep->lno ? 0 : ep->cno + 1;

	/* This is the loop that actually displays lines. */
	for (; len; --len) {
		/* Get the next character and figure out its length. */
		if ((ch = *p++) == '\t' && !ISSET(O_LIST))
			chlen = LVAL(O_TABSTOP) - count_cols % LVAL(O_TABSTOP);
		else
			chlen = asciilen[ch];
		count_cols += chlen;

		/*
		 * If skipping screens, see if crossed a screen boundary.  If
		 * so, and this is the last one to skip, start displaying the
		 * characters, assuming there's something to display.
		 */
		if (skip_cols) {
			if (count_cols < cols_per_screen) {
				if (cno_cnt)
					--cno_cnt;
				continue;
			}
			offset_in_char = chlen - (count_cols - cols_per_screen);
			chlen = count_cols -= cols_per_screen;
			if (--skip_cols || !chlen) {
				if (cno_cnt)
					--cno_cnt;
				continue;
			}
		} else
			offset_in_char = 0;

		/*
		 * Only display up to the right-hand column.  If reach it,
		 * we're done.  Only set the cursor value if the entire
		 * character is displayed.
		 */
		if (count_cols >= cols_per_screen) {
			chlen -= count_cols - cols_per_screen;
			if (count_cols > cols_per_screen)
				cno_cnt = 0;
			len = 1;		/* XXX 1, not 0, for loop. */
		}

		/*
		 * Display the character.  If it's a tab and tabs aren't some
		 * ridiculous length, do it fast.  (We do tab expansion here
		 * because curses doesn't have a way to set the tab length.)
		 */
#define	BLANKS	"                    "
		if (ch == '\t' && !ISSET(O_LIST)) {
			chlen -= offset_in_char;
			if (chlen <= sizeof(BLANKS) - 1)
				addnstr(BLANKS, chlen);
			else
				while (chlen--)
					addch(' ');
		} else
			addnstr(asciiname[ch] + offset_in_char, chlen);

		/*
		 * If the caller wants the cursor value, and this was the
		 * cursor character, set the value.
		 */
		if (cno_cnt && --cno_cnt == 0) {
			*xp = count_cols - 1;
			*yp = sp - HMAP;
		}
	}

	/*
	 * If O_LIST set, at the end of the line and didn't paint the whole
	 * line, add a trailing $.
	 */
	if (ISSET(O_LIST) && len == 0 && count_cols < cols_per_screen) {
		++count_cols;
		addch('$');
	}

	/* If didn't paint the whole line, clear the rest of it. */
	if (count_cols < cols_per_screen)
		clrtoeol();
	return (0);
}

/*
 * scr_modeline --
 *	Change the screen as necessary for a mode change, with refresh.
 */
int
scr_modeline(ep, isinput)
	EXF *ep;
	int isinput;
{
#define	RULERSIZE	15
#define	MODESIZE	(RULERSIZE + 15)

	static char buf[RULERSIZE];
	size_t oldy, oldx;

	getyx(stdscr, oldy, oldx);
	MOVE(ep, SCREENSIZE(ep), 0);
	clrtoeol();

	if (!ISSET(O_RULER) && !ISSET(O_SHOWMODE) || ep->cols <= RULERSIZE) {
		MOVE(ep, oldy, oldx);
		return (0);
	}

	/* Display the ruler and mode. */
	if (ISSET(O_RULER) && ep->cols > RULERSIZE) {
		MOVE(ep, SCREENSIZE(ep), ep->cols / 2 - RULERSIZE / 2);
		memset(buf, ' ', sizeof(buf) - 1);
		(void)snprintf(buf,
		    sizeof(buf) - 1, "%lu,%lu", ep->lno, ep->cno + 1);
		buf[strlen(buf)] = ' ';
		addstr(buf);
	}

	/* Show the mode. */
	if (ISSET(O_SHOWMODE) && ep->cols > MODESIZE) {
		MOVE(ep, SCREENSIZE(ep), ep->cols - 7);
		addstr(isinput ? "Input" : "Command");
	}
	MOVE(ep, oldy, oldx);
	return (0);
}

/*
 * scr_refresh --
 *	Repaint a logical line on the screen.
 */
int
scr_refresh(ep, loff)
	EXF *ep;
	size_t loff;
{
	SMAP *p;

	p = HMAP + loff;
	if (p > TMAP)
		return (1);
	scr_line(ep, p, NULL, 0, NULL, NULL);
}

/*
 * scr_screens --
 *	Return the number of screens required by the line, or, if a column
 *	is specified, by the column within the line.
 */
size_t
scr_screens(ep, lno, cnop)
	EXF *ep;
	recno_t lno;
	size_t *cnop;
{
	size_t cno_cnt, cols, lcnt, len, scno;
	int ch;
	u_char *p;

	/* Get a copy of the line. */
	if ((p = file_gline(ep, lno, &len)) == NULL || len == 0)
		return (1);

	/* If a line number is being displayed, there's fewer columns. */
	cols = ep->cols;
	if (ISSET(O_NUMBER))
		cols -= O_NUMBER_LENGTH;

	/*
	 * If a column specified, note it, and set the counter to the
	 * column plus one, so we can use 0 as a flag value.
	 */
	cno_cnt = cnop == NULL ? 0 : *cnop + 1;

	/* Calculate the lines needed. */
	for (lcnt = 1, scno = 0; len--;) {
		if ((ch = *p++) == '\t' && !ISSET(O_LIST))
			scno += LVAL(O_TABSTOP) - scno % LVAL(O_TABSTOP);
		else
			scno += asciilen[ch];

		if (len && scno >= cols) {
			scno -= cols;
			++lcnt;
		}
		if (cno_cnt && --cno_cnt == 0)
			break;
	}

	/* Trailing '$' on listed lines. */
	if (len == 0 && ISSET(O_LIST)) {
		scno += asciilen['$'];
		if (scno > cols)
			++lcnt;
	}
	return (lcnt);
}

/*
 * scr_relative --
 *	Return the physical column from the line that will display a character
 *	closest to the currently most attractive character position.  The vi
 *	functions use this routine because only screen routine knows how many
 *	columns each character needs.
 */
size_t
scr_relative(ep, lno)
	EXF *ep;
	recno_t lno;
{
	size_t cno, len, llen, scno;
	int ch;
	u_char *lp, *p;

	/* First non-blank character. */
	if (ep->rcmflags == RCM_FNB) {
		(void)nonblank(ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (ep->rcmflags != RCM_LAST && ep->scno == 0)
		return (0);

	/* Need the line to go any further. */
	if ((lp = file_gline(ep, lno, &len)) == NULL)
		return (0);

	/* Empty lines are easy. */
	if (len == 0)
		return (0);

	/* Last character is easy, and common. */
	if (ep->rcmflags == RCM_LAST)
		return (len - 1);

	/* Set scno to the right initial value. */
	scno = ISSET(O_NUMBER) ? O_NUMBER_LENGTH : 0;

	/* Step through the line until reach the right character. */
	for (p = lp, llen = len; --len;) {
		ch = *p++;
		if (ch == '\t' && !ISSET(O_LIST))
			scno += LVAL(O_TABSTOP) - scno % LVAL(O_TABSTOP);
		else
			scno += asciilen[ch];
		if (scno >= ep->rcm) {
			len = p - lp;
			return (scno == ep->rcm ? len : len - 1);
		}
	}
	return (llen - 1);
}
