/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_line.c,v 5.5 1993/04/13 16:30:14 bostic Exp $ (Berkeley) $Date: 1993/04/13 16:30:14 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"

/*
 * svi_line --
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
svi_line(sp, ep, smp, p, len, yp, xp)
	SCR *sp;
	EXF *ep;
	SMAP *smp;
	register char *p;
	size_t len, *xp, *yp;
{
	CHNAME *cname;
	size_t chlen, cols_per_screen, cno_cnt, count_cols;
	size_t offset_in_char, skip_cols;
	int ch;
	char nbuf[10];

	/* Move to the line. */
	MOVE(sp, smp - HMAP, 0);

	/*
	 * If O_NUMBER is set and this is the first screen of a folding
	 * line or any left-right line, display the line number.  Set
	 * the number of columns for this screen.
	 */
	if (O_ISSET(sp, O_NUMBER) &&
	    (O_ISSET(sp, O_LEFTRIGHT) || smp->off == 1)) {
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
		if (yp != NULL && smp->lno == sp->lno) {
			*xp = 0;
			*yp = smp - HMAP;
		}
		if (smp->lno > file_lline(sp, ep))
			addch(smp->lno == 1
			    ? O_ISSET(sp, O_LIST) ? '$' : ' ' : '~');
		else if (p == NULL) {
			GETLINE_ERR(sp, smp->lno);
			return (1);
		} else if (O_ISSET(sp, O_LIST))
			addch('$');
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
	cno_cnt = yp == NULL || smp->lno != sp->lno ? 0 : sp->cno + 1;

	/* This is the loop that actually displays lines. */
	cname = sp->cname;
	for (; len; --len) {
		/* Get the next character and figure out its length. */
		if ((ch = *(u_char *)p++) == '\t' && !O_ISSET(sp, O_LIST))
			chlen = TAB_OFF(sp, count_cols);
		else
			chlen = cname[ch].len;
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
		if (ch == '\t' && !O_ISSET(sp, O_LIST)) {
			chlen -= offset_in_char;
			if (chlen <= sizeof(BLANKS) - 1)
				addnstr(BLANKS, chlen);
			else
				while (chlen--)
					addch(' ');
		} else
			addnstr(cname[ch].name + offset_in_char, chlen);

		/*
		 * If the caller wants the cursor value, and this was the
		 * cursor character, set the value.
		 */
		if (cno_cnt && --cno_cnt == 0) {
			*xp = count_cols - 1;
			*yp = smp - HMAP;
		}
	}

	/* If O_LIST set and at the end of the line, add a trailing $. */
	if (O_ISSET(sp, O_LIST) && len == 0) {
		++count_cols;
		addch('$');
	}

	/* If didn't paint the whole line, clear the rest of it. */
	if (count_cols < cols_per_screen)
		clrtoeol();
	return (0);
}
