/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.11 1992/12/22 14:59:29 bostic Exp $ (Berkeley) $Date: 1992/12/22 14:59:29 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <curses.h>
#include <db.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"

/*
 * screen_init --
 *	Initialize curses, and draw the screen.
 */
int
scr_init(ep)
	EXF *ep;
{
	if (initscr() == NULL) {
		msg("Error: initscr failed: %s", strerror(errno));
		return (1);
	}
	raw();
	nonl();
	noecho();
	scrollok(stdscr, 1);
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
	endwin();		/* No delwin(), initscr() does them. */
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
 *
 * XXX
 *	Obvious race exists -- if receive SIGWINCH while updating the
 *	screen, or in msg() it's possible to fail badly?
 */
void
onwinch(signo)
	int signo;
{
	struct winsize win;
	char *argv[2], sbuf[100];

	/*
	 * Try TIOCGWINSZ.  If it fails, ignore the signal.  Otherwise,
	 * reset any environmental values, so curses uses the new values.
	 */
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) == -1) {
		msg("TIOCGWINSZ: %s", strerror(errno));
		return;
	}

	(void)unsetenv("ROWS");
	(void)unsetenv("COLUMNS");

	argv[0] = sbuf;
	argv[1] = NULL;

	(void)snprintf(sbuf, sizeof(sbuf), "ls=%u", win.ws_row);
	if (opts_set(argv))
		return;
	(void)snprintf(sbuf, sizeof(sbuf), "co=%u", win.ws_col);
	if (opts_set(argv))
		return;

	/* End the current screen. */
	(void)scr_end(curf);

	/* Set the new values and start the next one. */
	(void)snprintf(sbuf, sizeof(sbuf), "%u", win.ws_row);
	(void)setenv("ROWS", sbuf, 1);
	(void)snprintf(sbuf, sizeof(sbuf), "%u", win.ws_col);
	(void)setenv("COLUMNS", sbuf, 1);

	(void)scr_init(curf);

	curf->lines = LINES;
	curf->cols = COLS;

	FF_SET(curf, F_REDRAW);
	scr_cchange(curf);
	refresh();
}
