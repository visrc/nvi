/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.3 1992/10/20 18:25:01 bostic Exp $ (Berkeley) $Date: 1992/10/20 18:25:01 $";
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
 * scr_init --
 *	Initialize curses, and draw the screen.
 */
int
scr_init()
{
	if (initscr() == NULL)
		return (1);
	raw();
	noecho();
	nonl();
	scrollok(stdscr, 1);

	move(0, 0);
	return (0);
}

/*
 * scr_end --
 *	Move to the bottom of the screen, end curses.
 */
void
scr_end()
{
	if (move(LINES - 1, 0) != ERR) {
		clrtoeol();
		refresh();
	}
	endwin();
}

/*
 * scr_ref --
 *	Repaint the screen.
 */
void
scr_ref()
{
	register recno_t cnt;

	/* Repaint the screen. */
	for (cnt = BOTLINE; cnt >= curf->top; --cnt)
		scr_update(curf, cnt, NULL, 0, LINE_RESET);

	/* Put up the cursor, row/column information. */
	scr_cchange(curf);
	scr_modeline(curf, 0);
} 

/*
 * scr_modeline --
 *	Change the screen as necessary for a mode change, with refresh.
 */
void
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

	if (!ISSET(O_RULER) && !ISSET(O_SHOWMODE) || COLS <= RULERSIZE)
		return;

	getyx(stdscr, oldy, oldx);
	move(LINES - 1, 0);
	clrtoeol();

	/* Display the ruler and mode. */
	if (ISSET(O_RULER) && COLS > RULERSIZE &&
	    move(LINES - 1, COLS / 2 - RULERSIZE / 2) != ERR) {
		memset(buf, ' ', sizeof(buf) - 1);
		(void)snprintf(buf,
		    sizeof(buf) - 1, "%lu,%lu", ep->lno, ep->cno + 1);
		buf[strlen(buf)] = ' ';
		addstr(buf);
	}

	/* Show the mode. */
	if (ISSET(O_SHOWMODE) && COLS > MODESIZE &&
	    move(LINES - 1, COLS - 7) != ERR)
		addstr(isinput ? "Input" : "Command");
	move(oldy, oldx);
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
	scr_end();
	(void)scr_init();
	scr_ref();
	refresh();
}
