/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.42 1993/03/28 19:06:20 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:06:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"

static int	svi_modeline __P((SCR *, EXF *));
static int	svi_msgflush __P((SCR *));

/*
 * svi_refresh --
 *	This is the guts of the vi curses screen code.  The idea is that
 *	the EXF structure passed in contains the new coordinates of the
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
	 * function svi_end() uses the values from the EXF structure, and
	 * then svi_begin() uses the new values, from the curses package,
	 * which got them from the environment.  We hope.
	 *
	 * Once the resize is done, we fall through.  The reason for this is
	 * that RESIZE or REDRAW might be set along with another movement.
	 * So, fall through until we figure out what the map should look like
	 * and then jump to repaint.
	 */
	if (F_ISSET(sp, S_RESIZE)) {
		if (svi_end(sp) || svi_init(sp, ep))
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
		if (svi_sm_fill(sp, ep, HMAP->lno, P_TOP))
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
		lcnt = svi_sm_nlines(sp, ep, &tmp, lastline, TEXTSIZE(sp));
		if (lcnt < TEXTSIZE(sp)) {
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
	 * 3b: Line up.
	 *
	 * If less than half a screen away, scroll up until the desired
	 * line is the first line on the screen.
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
	 * At this point we know that part of the line is on the screen.
	 * Because scrolling is done using logical ines, not physical,
	 * all of the line may not be on the screen.  While that's not
	 * necessarily bad, if the part the cursor is on isn't there,
	 * we're going to lose.
	 */
adjust:	if (LNO == HMAP->lno) {
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

	/* Display any messages or paint the mode line. */
	if (sp->msgp != NULL && !(sp->msgp->flags & M_EMPTY))
		svi_msgflush(sp);
	else
		svi_modeline(sp, ep);

	/* Place the cursor. */
	MOVE(sp, y, SCNO);

	/* Update saved information. */
	OCNO = CNO;
	OLNO = LNO;

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
	MSG *mp;
	size_t len;
	int ch;
	char *p;

#define	MCONTMSG	" [More ...]"

	/* Display the messages. */
	for (mp = sp->msgp, p = NULL;
	    mp != NULL && !(mp->flags & M_EMPTY); mp = mp->next) {

		p = mp->mbuf;

lcont:		/* Move to the message line and clear it. */
		MOVE(sp, SCREENSIZE(sp), 0);
		clrtoeol();

		/* Turn on standout mode if requested. */
		if (mp->flags & (M_BELL | M_ERROR))
			standout();

		/*
		 * Figure out how much to print, and print it.
		 * Adjust for the next line.
		 */
		len = sp->cols;
		if (mp->next != NULL && !(mp->next->flags & M_EMPTY))
			len -= sizeof(MCONTMSG);
		if (mp->len < len)
			len = mp->len;
		addnstr(p, len);
		p += len;
		mp->len -= len;

		/* Turn off standout mode. */
		if (mp->flags & (M_BELL | M_ERROR))
			standend();

		/* If more, print continue message. */
		if (mp->len ||
		    mp->next != NULL && !(mp->next->flags & M_EMPTY)) {
			addnstr(MCONTMSG, sizeof(MCONTMSG) - 1);
			refresh();
			while (sp->special[ch = getkey(sp, 0)] != K_CR &&
			    !isspace(ch))
				svi_bell(sp);
		}
		if (mp->len)
			goto lcont;

		refresh();
		mp->flags |= M_EMPTY;
	}
	return (0);
}

/*
 * svi_modeline --
 *	Update the mode line.
 */
int
svi_modeline(sp, ep)
	SCR *sp;
	EXF *ep;
{
#define	RULERSIZE	15
#define	MODESIZE	(RULERSIZE + 15)
	char buf[RULERSIZE];

	MOVE(sp, SCREENSIZE(sp), 0);
	clrtoeol();

	if (!ISSET(O_RULER) && !ISSET(O_SHOWMODE) || sp->cols <= RULERSIZE)
		return (0);

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
		addstr(F_ISSET(sp, S_INPUT) ? "Input" : "Command");
	}
	return (0);
}
