/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.4 1992/10/24 14:22:46 bostic Exp $ (Berkeley) $Date: 1992/10/24 14:22:46 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <curses.h>
#include <db.h>
#include <limits.h>
#include <unistd.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "screen.h"
#include "extern.h"

/*
 * screen_init --
 *	Initialize curses, and draw the screen.
 */
int
scr_init(ep)
	EXF *ep;
{
	static int first = 1;

	if (first) {
		first = 0;
		if (initscr() == NULL)
			return (1);
		raw();
		noecho();
		nonl();
		scrollok(stdscr, 1);
	}

	ep->lines = LINES;
	ep->cols = COLS;

	MOVE(0, 0);
	return (0);
}

/*
 * scr_end --
 *	Move to the bottom of the screen, end curses.
 */
int
scr_end(ep)
	EXF *ep;
{
	MOVE(SCREENSIZE(ep), 0);
	clrtoeol();
	refresh();
	endwin();
	return (0);
}

/*
 * scr_ref --
 *	Repaint the screen.
 */
int
scr_ref(ep)
	EXF *ep;
{
	register recno_t cnt;

	/* Repaint the screen. */
	for (cnt = BOTLINE(ep); cnt >= ep->otop; --cnt)
		(void)scr_update(ep, cnt, NULL, 0, LINE_RESET);

	/* Put up the cursor, row/column information. */
	(void)scr_cchange(ep);
	(void)scr_modeline(ep, 0);
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
	int when;
	char *p;

	getyx(stdscr, oldy, oldx);
	MOVE(SCREENSIZE(ep), 0);
	clrtoeol();

	if (!ISSET(O_RULER) && !ISSET(O_SHOWMODE) || ep->cols <= RULERSIZE) {
		MOVE(oldy, oldx);
		return (0);
	}

	/* Display the ruler and mode. */
	if (ISSET(O_RULER) && ep->cols > RULERSIZE) {
		MOVE(SCREENSIZE(ep), ep->cols / 2 - RULERSIZE / 2);
		memset(buf, ' ', sizeof(buf) - 1);
		(void)snprintf(buf,
		    sizeof(buf) - 1, "%lu,%lu", ep->lno, ep->cno + 1);
		buf[strlen(buf)] = ' ';
		addstr(buf);
	}

	/* Show the mode. */
	if (ISSET(O_SHOWMODE) && ep->cols > MODESIZE) {
		MOVE(SCREENSIZE(ep), ep->cols - 7);
		addstr(isinput ? "Input" : "Command");
	}
	MOVE(oldy, oldx);
	return (0);
}

/*
 * onwinch --
 *	Handle SIGWINCH.
 */
void
onwinch(signo)
	int signo;
{
	struct winsize win;
	char sbuf[40];

	/*
	 * Try TIOCGWINSZ.  If it fails, ignore the signal.  Otherwise,
	 * reset any environmental values, so curses uses the new values.
	 */
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		if (getenv("ROWS") != NULL) {
			(void)snprintf(sbuf, sizeof(sbuf), "%u", win.ws_row);
			(void)setenv("ROWS", sbuf, 1);
		}
		if (getenv("COLUMNS") != NULL) {
			(void)snprintf(sbuf, sizeof(sbuf), "%u", win.ws_col);
			(void)setenv("COLUMNS", sbuf, 1);
		}
	}
	(void)scr_end(curf);
	(void)scr_init(curf);
	(void)scr_ref(curf);
	refresh();
}
