/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_line.c,v 8.2 1993/09/10 18:46:15 bostic Exp $ (Berkeley) $Date: 1993/09/10 18:46:15 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <string.h>

#include "vi.h"
#include "svi_screen.h"

#if DEBUG && 0
#define	TABCH	'-'
#define	TABSTR	"--------------------"
#else
#define	TABSTR	"                    "
#define	TABCH	' '
#endif

/*
 * svi_line --
 *	Update one line on the screen.  One nasty little side effect is
 *	that it returns the screen position for the current character.
 *	Not pretty, but this is the only routine that really knows what's
 *	out there.
 */
int
svi_line(sp, ep, smp, yp, xp)
	SCR *sp;
	EXF *ep;
	SMAP *smp;
	size_t *xp, *yp;
{
	CHNAME *cname;
	SMAP *tsmp;
	recno_t lno;
	size_t chlen, cols_per_screen, cno_cnt, count_cols;
	size_t len, offset_in_line, offset_in_char, skip_screens;
	size_t oldy, oldx;
	int ch, is_tab, listset, is_partial, reverse_video;
	char *p, nbuf[10];

#if DEBUG && 0
	TRACE(sp, "svi_line: row %u: line: %u off: %u\n",
	    smp - HMAP, smp->lno, smp->off);
#endif
	/*
	 * Move to the line.  This routine can be called by svi_sm_position(),
	 * which uses it to fill in the cache entry so it can figure out what
	 * the real contents of the screen are.  Because of this, we have to
	 * return to whereever we started from.
	 */
	getyx(stdscr, oldy, oldx);
	MOVE(sp, smp - HMAP, 0);

	/* Get the character map. */
	cname = sp->cname;

	/*
	 * Special case if we're printing the info/mode line.  Skip printing
	 * the leading number, as well as other minor setup.  If painting the
	 * line between two screens, it's always in reverse video.  The only
	 * time this code paints the mode line is when the user is entering
	 * text for a ":" command, so we can put the code here instead of
	 * dealing with the empty line logic below.  This is a kludge, but it's
	 * pretty much confined to this module.
	 *
	 * Set the number of screens to skip until a character is displayed.
	 * Left-right screens are special, because we don't bother building
	 * a buffer to be skipped over.
	 *
	 * If O_NUMBER is set and this is the first screen of a folding
	 * line or any left-right line, display the line number.  Set
	 * the number of columns for this screen.
	 */
	reverse_video = 0;
	cols_per_screen = sp->cols;
	if (ISINFOLINE(sp, smp)) {
		if (sp->child != NULL) {
			reverse_video = 1;
			standout();
		}
		listset = 0;
		if (O_ISSET(sp, O_LEFTRIGHT))
			skip_screens = 0;
		else
			skip_screens = smp->off - 1;
	} else {
		listset = O_ISSET(sp, O_LIST);
		skip_screens = smp->off - 1;

		if (O_ISSET(sp, O_NUMBER) && skip_screens == 0) {
			cols_per_screen -= O_NUMBER_LENGTH;
			(void)snprintf(nbuf,
			    sizeof(nbuf), O_NUMBER_FMT, smp->lno);
			ADDSTR(nbuf);
		}
	}

	/*
	 * Get a copy of the line.  Special case non-existent lines and the
	 * first line of an empty file.  In both cases, the cursor position
	 * is 0.
	 */
	if ((p = file_gline(sp, ep, smp->lno, &len)) == NULL || len == 0) {
		if (yp != NULL && smp->lno == sp->lno) {
			*yp = smp - HMAP;
			*xp = O_ISSET(sp, O_NUMBER) ? O_NUMBER_LENGTH : 0;
		}
		if (file_lline(sp, ep, &lno))
			goto err;
		if (smp->lno > lno) {
			ADDCH(smp->lno == 1 ?
			    listset && skip_screens == 0 ? '$' : ' ' : '~');
		} else if (p == NULL) {
			GETLINE_ERR(sp, smp->lno);
err:			MOVEA(sp, oldy, oldx);
			return (1);
		} else if (listset && skip_screens == 0)
			ADDCH('$');
		clrtoeol();
		MOVEA(sp, oldy, oldx);
		return (0);
	}

	/*
	 * If we wrote a line that's this or a previous one, we can do this
	 * much more quickly -- we cached the starting and ending positions
	 * of that line.  The way it works is we keep information about the
	 * lines displayed in the SMAP.  If we're painting the screen in
	 * the forward, this saves us from reformatting the physical line for
	 * every line on the screen.  This wins big on binary files with 10K
	 * lines.
	 *
	 * Test for the first screen of the line, then the current screen line,
	 * then the line behind us, then do the hard work.  Note, it doesn't
	 * do us any good to have a line in front of us -- it would be really
	 * hard to try and figure out tabs in the reverse direction, i.e. how
	 * many spaces a tab takes up in the reverse direction depends on
	 * what characters preceded it.
	 */
	if (smp->off == 1) {
		smp->c_sboff = offset_in_line = 0;
		smp->c_scoff = offset_in_char = 0;
		p = &p[offset_in_line];
	} else if (SMAP_CACHE(smp)) {
		offset_in_line = smp->c_sboff;
		offset_in_char = smp->c_scoff;
		p = &p[offset_in_line];
	} else if (smp != HMAP &&
	    SMAP_CACHE(tsmp = smp - 1) && tsmp->lno == smp->lno) {
		if (tsmp->c_eclen != tsmp->c_ecsize) {
			offset_in_line = tsmp->c_eboff;
			offset_in_char = tsmp->c_ecsize - tsmp->c_eclen;
		} else {
			offset_in_line = tsmp->c_eboff + 1;
			offset_in_char = 0;
		}

		/* Put starting info for this line in the cache. */
		smp->c_sboff = offset_in_line;
		smp->c_scoff = offset_in_char;
		p = &p[offset_in_line];
	} else {
		offset_in_line = 0;
		offset_in_char = 0;

		/* This is the loop that skips through screens. */
		if (skip_screens == 0) {
			smp->c_sboff = offset_in_line;
			smp->c_scoff = offset_in_char;
		} else for (count_cols = 0;
		    offset_in_line < len; ++offset_in_line) {
			chlen = (ch = *(u_char *)p++) == '\t' && !listset ?
			    TAB_OFF(sp, count_cols) : cname[ch].len;
			count_cols += chlen;

			/*
			 * If crossed the last skipped screen boundary,
			 * start displaying the characters.
			 */
			if (count_cols < cols_per_screen)
				continue;
			count_cols -= cols_per_screen;
			if (--skip_screens)
				continue;

			/* Put starting info for this line in the cache. */
			if (count_cols) {
				smp->c_sboff = offset_in_line;
				smp->c_scoff =
				    offset_in_char = chlen - count_cols;
			} else {
				smp->c_sboff = ++offset_in_line;
				smp->c_scoff = 0;
			}
			break;
		}
	}

	/*
	 * Set the number of characters to skip before reaching the cursor
	 * character.  Offset by 1 and use 0 as a flag value.  Svi_line is
	 * called repeatedly with a valid pointer to a cursor position.
	 * Don't fill anything in unless it's the right line and the right
	 * character, and the right part of the character...
	 */
	if (yp == NULL || smp->lno != sp->lno || sp->cno < offset_in_line ||
	    offset_in_line + cols_per_screen < sp->cno)
		cno_cnt = 0;
	else
		cno_cnt = (sp->cno - offset_in_line) + 1;

	/* This is the loop that actually displays characters. */
	for (is_partial = 0, count_cols = 0;
	    offset_in_line < len; ++offset_in_line) {
		if ((ch = *(u_char *)p++) == '\t' && !listset) {
			chlen = TAB_OFF(sp, count_cols) - offset_in_char;
			is_tab = 1;
		} else {
			chlen = cname[ch].len - offset_in_char;
			is_tab = 0;
		}
		count_cols += chlen;

		/*
		 * Only display up to the right-hand column.  Set a flag if
		 * the entire character wasn't displayed for use in setting
		 * the cursor.  If we filled the screen, set the cache info
		 * for the next screen.  Don't worry about there not being
		 * characters to display on the next screen, it's lno/off
		 * won't match up in that case.
		 */
		if (count_cols >= cols_per_screen) {
			smp->c_ecsize = chlen;
			chlen -= count_cols - cols_per_screen;
			smp->c_eclen = chlen;
			smp->c_eboff = offset_in_line;
			if (count_cols > cols_per_screen)
				is_partial = 1;

			/* Terminate the loop. */
			offset_in_line = len;
		}

		/*
		 * If the caller wants the cursor value, and this was the
		 * cursor character, set the value.  There are two ways to
		 * put the cursor on a character -- if it's normal display
		 * mode, it goes on the last column of the character.  If
		 * it's input mode, it goes on the first.  In normal mode,
		 * set the cursor only if the entire character was displayed.
		 */
		if (cno_cnt && --cno_cnt == 0 &&
		    (F_ISSET(sp, S_INPUT) || !is_partial)) {
			*yp = smp - HMAP;
			if (F_ISSET(sp, S_INPUT))
				*xp = count_cols - chlen;
			else
				*xp = count_cols - 1;
			if (O_ISSET(sp, O_NUMBER) &&
			    !ISINFOLINE(sp, smp) && smp->off == 1)
				*xp += O_NUMBER_LENGTH;
		}

		/*
		 * Display the character.  If it's a tab and tabs aren't some
		 * ridiculous length, do it fast.  (We do tab expansion here
		 * because curses doesn't have a way to set the tab length.)
		 */
		if (is_tab) {
			if (chlen <= sizeof(TABSTR) - 1) {
				ADDNSTR(TABSTR, chlen);
			} else
				while (chlen--)
					ADDCH(TABCH);
		} else
			ADDNSTR(cname[ch].name + offset_in_char, chlen);
		offset_in_char = 0;
	}

	/*
	 * If not the info/mode line, and O_LIST set, and at the end of
	 * the line, and the line ended on this screen, add a trailing $.
	 */
	if (listset && offset_in_line == len) {
		++count_cols;
		ADDCH('$');
	}

	/* If didn't paint the whole line, clear the rest of it. */
	if (count_cols < cols_per_screen)
		clrtoeol();

	if (reverse_video)
		standend();
	MOVEA(sp, oldy, oldx);
	return (0);
}
