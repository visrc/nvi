/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_refresh.c,v 5.12 1992/12/22 16:10:34 bostic Exp $ (Berkeley) $Date: 1992/12/22 16:10:34 $";
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
 */
void
onwinch(signo)
	int signo;
{
	struct winsize win;
	char *argv[2], sbuf[100];

	/*
	 * Try TIOCGWINSZ.  If it fails, ignore the signal.  Otherwise,
	 * set the row/column options.  No error messages, because it's
	 * not worth making msg reentrant.
	 */
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) == -1)
		return;

	argv[0] = sbuf;
	argv[1] = NULL;

	(void)snprintf(sbuf, sizeof(sbuf), "ls=%u", win.ws_row);
	if (opts_set(argv))
		return;
	(void)snprintf(sbuf, sizeof(sbuf), "co=%u", win.ws_col);
	if (opts_set(argv))
		return;

	/* Do the resize if waiting, otherwise just schedule it. */
	FF_SET(curf, F_RESIZE);
	if (FF_ISSET(curf, F_READING)) {
		scr_cchange(curf);
		refresh();
	}
}
