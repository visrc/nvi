/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.41 1993/03/26 13:40:41 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:40:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"

/*
 * This code is the historic screen interface for the vi editor, with the
 * one change that it supports left-right scrolling.  It's commented heavily
 * because it's downright nasty.  Changing this code can be hazardous to
 * your health.
 *
 * We use a single curses window for vi.  The model would be simpler with
 * two windows (one for the text, and one for the modeline) because
 * scrolling the text window down would work correctly then, not affecting
 * the mode line.  As it is we have to play games to make it look right.
 * The reason for this choice is that it would be difficult for curses to
 * optimize the movement, i.e. detect that the downward scroll isn't going
 * to change the modeline, set the scrolling region on the terminal and only
 * scroll the first part of the text window.  (Even if curses did detect it,
 * the set-scrolling-region terminal commands can't be used by curses because
 * it's indeterminate where the cursor ends up after they are sent.)
 */

#define	HALFSCREEN(sp)	(SCREENSIZE(sp) / 2)	/* Half a screen. */

#define	HMAP		(sp->h_smap)		/* Head of line map. */
#define	TMAP		(sp->t_smap)		/* Tail of line map. */

/*
 * scr_def --
 *	Do the default initialization of an SCR structure.
 */
int
scr_def(orig, sp)
	SCR *orig, *sp;
{
	extern u_char asciilen[];		/* XXX */
	extern char *asciiname[];		/* XXX */

	memset(sp, 0, sizeof(SCR));		/* Zero out the structure. */

	if (orig != NULL) {			/* Copy some stuff. */
#ifdef notdef
		tag_copy(orig, sp);
		seq_copy(orig, sp);
		opt_copy(orig, sp);
#endif
		sp->confirm = orig->confirm;
		sp->end = orig->end;
		sp->gp = orig->gp;

		sp->searchdir = orig->searchdir;
		sp->csearchdir = orig->csearchdir;
		sp->inc_lastch = orig->inc_lastch;
		sp->inc_lastval = orig->inc_lastval;

		HDR_APPEND(sp, orig, next, prev, SCR);
	} else {
		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;
		sp->inc_lastch = '+';
		sp->inc_lastval = 1;
	}

	sp->clen = asciilen;			/* XXX */
	sp->cname = asciiname;			/* XXX */
	F_SET(sp, S_REDRAW);

	return (0);
}

/*
 * vi_s_init --
 *	Initialize curses and the screen.
 */
int
vi_s_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	void *p;

	if (initscr() == NULL) {
		msgq(sp,
		    M_ERROR, "Error: initscr failed: %s", strerror(errno));
		return (1);
	}
	noecho();			/* Don't echo characters. */
	nonl();				/* Don't do newline mapping. */
	raw();				/* No special characters. */
	scrollok(stdscr, 1);		/* Scrolling is encouraged. */
	idlok(stdscr, 1);		/* Use hardware scrolling. */

	sp->lines = LINES;		/* XXX: Way ugly. */
	sp->cols = COLS;

	/* Create the screen map. */
	if ((p = malloc(sp->lines * sizeof(SMAP))) == NULL)
		return (1);

	/* Put the map into the EXF structure. */
	HMAP = (SMAP *)p;
	TMAP = (SMAP *)p + TEXTSIZE(sp);

	(void)scr_sm_fill(sp, ep, ep->lno, P_FILL);

	F_CLR(sp, S_REFORMAT | S_RESIZE);
	F_SET(sp, S_REDRAW);

	return (0);
}

/*
 * vi_s_end --
 *	Move to the bottom of the screen, end curses.
 */
int
vi_s_end(sp)
	SCR *sp;
{
	if (HMAP == NULL)
		return (0);

	if (move(SCREENSIZE(sp), 0) == OK) {
		clrtoeol();
		refresh();
	}
	endwin();			/* No delwin(), initscr() does them. */

	if (HMAP != NULL) {
		free(HMAP);
		HMAP = NULL;
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
scr_change(sp, ep, lno, op)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum operation op;
{
	size_t oldy, oldx;

	/* Appending is the same as inserting, if the line is incremented. */
	if (op == LINE_APPEND)
		++lno;

	/* Ignore the change if the line is not on the screen. */
	if (lno < HMAP->lno || lno > TMAP->lno)
		return (0);

	getyx(stdscr, oldy, oldx);

	switch(op) {
	case LINE_DELETE:
		/* Update SMAP. */
		if (scr_sm_delete(sp, ep, lno))
			return (1);

		/* Invalidate the cursor. */
		ep->olno = OOBLNO;
		break;

	case LINE_APPEND:
	case LINE_INSERT:
		/* Update SMAP. */
		if (scr_sm_insert(sp, ep, lno))
			return (1);

		/* Invalidate the cursor. */
		ep->olno = OOBLNO;
		break;

	case LINE_RESET:
		/*
		 * Update SMAP.
		 *
		 * XXX
		 * Add an scr_sm_reset() function.
		 */
		if (scr_sm_delete(sp, ep, lno))
			return (1);
		if (scr_sm_insert(sp, ep, lno))
			return (1);
		break;
	default:
		abort();
	}

	MOVE(sp, oldy, oldx);

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
scr_update(sp, ep)
	SCR *sp;
	EXF *ep;
{
	SMAP *smp, tmp;
	recno_t lastline, lcnt;
	size_t cwtotal, cnt, len, x, y;
	int ch;
	u_char *clenp, *p;

#define	 LNO	ep->lno
#define	OLNO	ep->olno
#define	 CNO	ep->cno
#define	OCNO	ep->ocno
#define	SCNO	sp->scno

	/*
	 * 1: Resize the window.
	 *
	 * The actual changing of the row/column values was done by calling
	 * the ex options code which entered them into the environment.  The
	 * function scr_end() uses the values from the EXF structure, and
	 * then scr_begin() uses the new values, from the curses package,
	 * which got them from the environment.  We hope.
	 *
	 * Once the resize is done, we fall through.  The reason for this is
	 * that RESIZE or REDRAW might be set along with another movement.
	 * So, fall through until we figure out what the map should look like
	 * and then jump to repaint.
	 */
	if (F_ISSET(sp, S_RESIZE)) {
		if (vi_s_end(sp) || vi_s_init(sp, ep))
			return (1);

		F_CLR(sp, S_RESIZE);
		F_SET(sp, S_REFORMAT | S_REDRAW);
	}

	/*
	 * 2: Reformat the lines.
	 *
	 * If the lines themselves have changed (:set list, for example),
	 * fill in the map from scratch.
	 */
	if (F_ISSET(sp, S_REFORMAT)) {
		if (scr_sm_fill(sp, ep, HMAP->lno, P_TOP))
			return (1);

		F_CLR(sp, S_REFORMAT);
		F_SET(sp, S_REDRAW);
	}

	/*
	 * 3: Line movement.
	 *
	 * Line changes can cause the top line to change as well.  As
	 * before, if the movement is large, the screen is repainted.
	 *
	 * 3a: Line down.
	 */
	if (LNO >= HMAP->lno) {
		if (LNO <= TMAP->lno)
			goto adjust;

		/*
		 * If less than half a screen away, scroll down until the
		 * desired line is on the screen.
		 */
		lcnt = scr_sm_nlines(sp, ep, TMAP, LNO, HALFSCREEN(sp));
		if (lcnt < HALFSCREEN(sp)) {
			while (lcnt--)
				if (scr_sm_1up(sp, ep))
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
		lcnt = scr_sm_nlines(sp, ep, &tmp, lastline, TEXTSIZE(sp));
		if (lcnt < TEXTSIZE(sp)) {
			if (scr_sm_fill(sp, ep, lastline, P_BOTTOM))
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
	 * 3b: Line up.
	 *
	 * If less than half a screen away, scroll up until the desired
	 * line is the first line on the screen.
	 */
	lcnt = scr_sm_nlines(sp, ep, HMAP, LNO, HALFSCREEN(sp));
	if (lcnt < HALFSCREEN(sp)) {
		while (lcnt--)
			if (scr_sm_1down(sp, ep))
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
	lcnt = scr_sm_nlines(sp, ep, &tmp, LNO, HALFSCREEN(sp));
	if (lcnt < HALFSCREEN(sp))
		(void)scr_sm_fill(sp, ep, 1, P_TOP);
	else
middle:		(void)scr_sm_fill(sp, ep, LNO, P_MIDDLE);

	/*
	 * If we got here, we know the screen needs to be repainted, in
	 * which case, we can skip any cursor optimization.
	 */
	goto paint;

	/*
	 * At this point we know that part of the line is on the screen.
	 * Because scrolling is done using logical ines, not physical,
	 * all of the line may not be on the screen.  While that's not
	 * necessarily bad, if the part the cursor is on isn't there,
	 * we're going to lose.
	 */
adjust:	if (LNO == HMAP->lno) {
		cnt = scr_screens(sp, ep, LNO, &CNO);
		while (cnt < HMAP->off)
			if (scr_sm_1down(sp, ep))
				return (1);
	} else if (LNO == TMAP->lno) {
		cnt = scr_screens(sp, ep, LNO, &CNO);
		while (cnt > TMAP->off)
			if (scr_sm_1up(sp, ep))
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
	if (F_ISSET(sp, (S_CHARDELETED | S_CUR_INVALID))) {
		F_CLR(sp, S_CHARDELETED | S_CUR_INVALID);
		goto slow;
	}

	/* If the line we're working with has changed, reparse. */
	if (LNO != OLNO)
		goto slow;

	/* If no cursor movement, don't trust it. */
	if (CNO != OCNO)
		goto slow;

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
		return (0);
	}

	/*
	 * The basic scheme here is to look at the characters in between
	 * the old and new positions and decide how big they are on the
	 * screen, and therefore, how many screen positions to move.
	 */
	clenp = sp->clen;
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
		 * character, just do it the the slow way.
		 */
		for (cwtotal = 0; cnt--; cwtotal += clenp[ch])
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
		if (clenp[ch] > 1)
			cwtotal -= clenp[ch] - 1;

		/*
		 * If the new column moved us out of the current screen,
		 * calculate a new screen.
		 */
		if (SCNO < cwtotal) {
			if (ISSET(O_LEFTRIGHT)) {
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
		 * Point to the old character.
		 */
		p += OCNO;
		cnt = (CNO - OCNO) + 1;
#ifdef DEBUG
		if (CNO >= len) {
			msgq(sp, M_ERROR,
			    "Error: %s/%d: cno (%u) >= len (%u)",
			     tail(__FILE__), __LINE__, CNO, len);
			return (1);
		}
#endif
		/* Save the width of the original character. */
		len = clenp[*p];

		/*
		 * Count up the widths of the characters.  If it's a tab
		 * character, just do it the the slow way.
		 */
		for (cwtotal = 0; cnt--; cwtotal += clenp[ch])
			if ((ch = *p++) == '\t')
				goto slow;

		/*
		 * Increment the screen cursor by the total width of the
		 * characters minus the width of the original character.
		 */
		SCNO += cwtotal - len;

		/*
		 * If the new column moved us out of the current screen,
		 * calculate a new screen.
		 */
		if (SCNO >= SCREEN_COLS(sp)) {
			if (ISSET(O_LEFTRIGHT)) {
				SCNO -= SCREEN_COLS(sp);
				for (smp = HMAP; smp <= TMAP; ++smp)
					++smp->off;
				goto paint;
			}
			goto slow;
		}
	}

	/* It was easy, just move the cursor. */
	getyx(stdscr, y, x);
	goto update;

slow:	/* Find the current line in the map. */
	for (smp = HMAP; smp->lno != LNO; ++smp);

	/*
	 * If doing left-right scrolling, and the cursor movement has
	 * changed the screen being displayed, fix it.
	 */
	if (ISSET(O_LEFTRIGHT)) {
		cnt = scr_screens(sp, ep, LNO, &CNO) % SCREEN_COLS(sp);
		if (cnt != HMAP->off) {
			for (smp = HMAP; smp <= TMAP; ++smp)
				smp->off = cnt;
			goto paint;
		}
	}

	/* Update all of the screen lines for this file line. */
	for (; smp <= TMAP && smp->lno == LNO; ++smp)
		if (scr_line(sp, ep, smp, NULL, 0, &y, &SCNO))
			return (1);

	/* Not too bad, move the cursor. */
	goto update;

	/* Lost big, do what you have to do. */
paint:	for (smp = HMAP; smp <= TMAP; ++smp)
		if (scr_line(sp, ep, smp, NULL, 0, &y, &SCNO))
			return (1);
	F_CLR(sp, S_REDRAW);
		
update:	MOVE(sp, y, SCNO);

	/* Refresh the screen if necessary. */
	if (F_ISSET(sp, S_REFRESH)) {
		wrefresh(curscr);
		F_CLR(sp, S_REFRESH);
	}

	/* Update saved information. */
	OCNO = CNO;
	OLNO = LNO;

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
scr_line(sp, ep, smp, p, len, yp, xp)
	SCR *sp;
	EXF *ep;
	SMAP *smp;
	register u_char *p;
	size_t len, *xp, *yp;
{
	size_t chlen, cols_per_screen, cno_cnt, count_cols;
	size_t offset_in_char, skip_cols;
	int ch;
	u_char *clenp;
	char **cnamep, nbuf[10];

	/* Move to the line. */
	MOVE(sp, smp - HMAP, 0);

	/*
	 * If O_NUMBER is set and this is the first screen of a folding
	 * line or any left-right line, display the line number.  Set
	 * the number of columns for this screen.
	 */
	if (ISSET(O_NUMBER) && (ISSET(O_LEFTRIGHT) || smp->off == 1)) {
		cols_per_screen = sp->cols -
		    snprintf(nbuf, sizeof(nbuf), O_NUMBER_FMT, smp->lno);
		addstr(nbuf);
	} else
		cols_per_screen = sp->cols;

	/*
	 * Get a copy of the line.  Special case non-existent lines and the
	 * first line of an empty file.  In both cases, the cursor position
	 * is 0.
	 */
	if (p == NULL)
		p = file_gline(sp, ep, smp->lno, &len);
	if (p == NULL || len == 0) {
		if (yp != NULL && smp->lno == ep->lno) {
			*xp = 0;
			*yp = smp - HMAP;
		}
		if (smp->lno > file_lline(sp, ep))
			addch(smp->lno == 1 ? ISSET(O_LIST) ? '$' : ' ' : '~');
		else if (p == NULL) {
			GETLINE_ERR(sp, smp->lno);
			return (1);
		}
		clrtoeol();
		return (0);
	}

	/*
	 * Set the number of column positions to skip until a character
	 * gets displayed.
	 */
	skip_cols = smp->off - 1;
	count_cols = 0;

	/*
	 * Set the number of characters to skip before reach the cursor
	 * character.  Offset by 1 and use 0 as a flag value.  We may be
	 * called repeatedly with a valid pointer to a cursor position.
	 * Don't fill it in unless it's the right line.
	 */
	cno_cnt = yp == NULL || smp->lno != ep->lno ? 0 : ep->cno + 1;

	/* This is the loop that actually displays lines. */
	clenp = sp->clen;
	cnamep = sp->cname;
	for (; len; --len) {
		/* Get the next character and figure out its length. */
		if ((ch = *p++) == '\t' && !ISSET(O_LIST))
			chlen = LVAL(O_TABSTOP) - count_cols % LVAL(O_TABSTOP);
		else
			chlen = clenp[ch];
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
			addnstr(cnamep[ch] + offset_in_char, chlen);

		/*
		 * If the caller wants the cursor value, and this was the
		 * cursor character, set the value.
		 */
		if (cno_cnt && --cno_cnt == 0) {
			*xp = count_cols - 1;
			*yp = smp - HMAP;
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
scr_modeline(sp, ep, isinput)
	SCR *sp;
	EXF *ep;
	int isinput;
{
#define	RULERSIZE	15
#define	MODESIZE	(RULERSIZE + 15)

	size_t oldy, oldx;
	char buf[RULERSIZE];

	getyx(stdscr, oldy, oldx);
	MOVE(sp, SCREENSIZE(sp), 0);
	clrtoeol();

	if (!ISSET(O_RULER) &&
	    !ISSET(O_SHOWMODE) || sp->cols <= RULERSIZE) {
		MOVE(sp, oldy, oldx);
		return (0);
	}

	/* Display the ruler and mode. */
	if (ISSET(O_RULER) && sp->cols > RULERSIZE) {
		MOVE(sp, SCREENSIZE(sp), sp->cols / 2 - RULERSIZE / 2);
		memset(buf, ' ', sizeof(buf) - 1);
		(void)snprintf(buf,
		    sizeof(buf) - 1, "%lu,%lu", ep->lno, ep->cno + 1);
		buf[strlen(buf)] = ' ';
		addstr(buf);
	}

	/* Show the mode. */
	if (ISSET(O_SHOWMODE) && sp->cols > MODESIZE) {
		MOVE(sp, SCREENSIZE(sp), sp->cols - 7);
		addstr(isinput ? "Input" : "Command");
	}
	MOVE(sp, oldy, oldx);
	return (0);
}

/*
 * scr_refresh --
 *	Repaint a logical line on the screen.
 */
int
scr_refresh(sp, ep, loff)
	SCR *sp;
	EXF *ep;
	size_t loff;
{
	SMAP *p;

	p = HMAP + loff;
	if (p > TMAP)
		return (1);
	return (scr_line(sp, ep, p, NULL, 0, NULL, NULL));
}

/*
 * scr_screens --
 *	Return the number of screens required by the line, or, if a column
 *	is specified, by the column within the line.
 */
size_t
scr_screens(sp, ep, lno, cnop)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t *cnop;
{
	size_t cno_cnt, cols, lcnt, len, scno;
	int ch;
	u_char *clenp, *p;

	/* Get a copy of the line. */
	if ((p = file_gline(sp, ep, lno, &len)) == NULL || len == 0)
		return (1);

	/* If a line number is being displayed, there's fewer columns. */
	cols = sp->cols;
	if (ISSET(O_NUMBER))
		cols -= O_NUMBER_LENGTH;

	/*
	 * If a column specified, note it, and set the counter to the
	 * column plus one, so we can use 0 as a flag value.
	 */
	cno_cnt = cnop == NULL ? 0 : *cnop + 1;

	/* Calculate the lines needed. */
	clenp = sp->clen;
	for (lcnt = 1, scno = 0; len--;) {
		if ((ch = *p++) == '\t' && !ISSET(O_LIST))
			scno += LVAL(O_TABSTOP) - scno % LVAL(O_TABSTOP);
		else
			scno += clenp[ch];

		if (len && scno >= cols) {
			scno -= cols;
			++lcnt;
		}
		if (cno_cnt && --cno_cnt == 0)
			break;
	}

	/* Trailing '$' on listed lines. */
	if (len == 0 && ISSET(O_LIST)) {
		scno += clenp['$'];
		if (scno > cols)
			++lcnt;
	}
	return (lcnt);
}

/*
 * scr_relative --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position.  If it's not easy, uses the underlying routine that
 *	really figures it out.  It's broken into two parts because the
 *	scr_lrelative routine handles "logical" offsets, which nobody
 *	but the screen routines understand.
 */
size_t
scr_relative(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	size_t cno;

	/* First non-blank character. */
	if (sp->rcmflags == RCM_FNB) {
		(void)nonblank(sp, ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (sp->rcmflags != RCM_LAST && sp->scno == 0)
		return (0);

	return (scr_lrelative(sp, ep, lno, 1));
}

/*
 * scr_lrelative --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position.  The offset is for the commands that move logical
 *	distances, i.e. if it's a logical scroll the closest physical
 *	distance is based on the logical line, not the physical line.
 */
size_t
scr_lrelative(sp, ep, lno, off)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t off;
{
	size_t len, llen, scno;
	int ch;
	u_char *clenp, *lp, *p;

	/* Need the line to go any further. */
	if ((lp = file_gline(sp, ep, lno, &len)) == NULL)
		return (0);

	/* Empty lines are easy. */
	if (len == 0)
		return (0);

	/* Last character is easy, and common. */
	if (sp->rcmflags == RCM_LAST)
		return (len - 1);

	/* Set scno to the right initial value. */
	scno = ISSET(O_NUMBER) ? O_NUMBER_LENGTH : 0;

	/* Discard logical lines. */
	clenp = sp->clen;
	for (p = lp, llen = len; --off;) {
		for (; len && scno < sp->cols; --len) {
			ch = *p++;
			if (ch == '\t' && !ISSET(O_LIST))
				scno +=
				    LVAL(O_TABSTOP) - scno % LVAL(O_TABSTOP);
			else
				scno += clenp[ch];
		}
		if (len == 0)
			return (llen - 1);
		scno -= sp->cols;
	}

	/* Step through the line until reach the right character. */
	while (len--) {
		ch = *p++;
		if (ch == '\t' && !ISSET(O_LIST))
			scno += LVAL(O_TABSTOP) - scno % LVAL(O_TABSTOP);
		else
			scno += clenp[ch];
		if (scno >= sp->rcm) {
			len = p - lp;
			return (scno == sp->rcm ? len : len - 1);
		}
	}
	return (llen - 1);
}

/*
 * gdbrefresh --
 *	Stub routine so can step through screen changes.
 */
#ifdef DEBUG
int
gdbrefresh()
{
	refresh();
	return (0);
}
#endif
