/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.6 1992/10/24 14:27:13 bostic Exp $ (Berkeley) $Date: 1992/10/24 14:27:13 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"
#include "extern.h"

/*
 * v_eof --
 *	Vi end-of-file error.
 */
void
v_eof(mp)
	MARK *mp;
{
	u_long lno;

	bell();
	if (ISSET(O_VERBOSE))
		if (mp == NULL)
			msg("Already at end-of-file.");
		else {
			lno = file_lline(curf);
			if (mp->lno >= lno)
				msg("Already at end-of-file.");
			else
				msg("Movement past the end-of-file.");
		}
}

/*
 * v_eol --
 *	Vi end-of-line error.
 */
void
v_eol(mp)
	MARK *mp;	
{
	size_t len;
	u_char *p;

	bell();
	if (ISSET(O_VERBOSE))
		if (mp == NULL)
			msg("Already at end-of-line.");
		else {
			if ((p = file_gline(curf, mp->lno, &len)) == NULL) {
				GETLINE_ERR(mp->lno);
				return;
			}
			if (mp->cno == len - 1)
				msg("Already at end-of-line.");
			else
				msg("Movement past the end-of-line.");
		}
}

/*
 * v_sof --
 *	Vi start-of-file error.
 */
void
v_sof(mp)
	MARK *mp;
{
	bell();
	if (ISSET(O_VERBOSE))
		if (mp == NULL || mp->lno == 1)
			msg("Already at the beginning of the file.");
		else
			msg("Movement past the beginning of the file.");
}
		
static u_long oldy, oldx;
static struct termios save;

/*
 * v_startex --
 *	Enter into ex mode.
 */
int
v_startex()
{
	struct termios t;

	getyx(stdscr, oldy, oldx);

	/* Go to the ex window line and clear it. */
	MOVE(SCREENSIZE(curf), 0);
	clrtoeol();
	refresh();

	/* Save the curses state. */
	(void)tcgetattr(STDIN_FILENO, &save);

	/* Suspend the window. */
	endwin();

	/*
	 * XXX
	 * These lines know much too much about the tty driver.  We want raw
	 * input, but we want NL -> CR mapping on output.  The only way to get
	 * that in the 4.4BSD tty driver is to have OPOST turned on and it's
	 * currently turned off by cfmakeraw(3).
	 */
	(void)tcgetattr(STDIN_FILENO, &t);
	cfmakeraw(&t);
	t.c_oflag |= ONLCR|OPOST;
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &t);

	/* Initialize the global that let vi know what happened in ex. */
	ex_prstate = PR_NONE;

	return (0);
}

/*
 * v_leaveex --
 *	Leave into ex mode.
 */
int
v_leaveex()
{
	int cnt;

	/* Make sure ex got everything out. */
	(void)fflush(stdout);

	/* Switch back to the curses termios values. */
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &save);

	/* Put the cursor back. */
	MOVE(oldy, oldx);
TRACE("v_leaveex: move to y %u x %u\n", oldy, oldx);

#ifdef notdef
	/*
	 * Clear the mode line.
	 *
 	 * XXX
	 * We have to clear the line even though the curses package doesn't
	 * know that anything is there.  So, we hack the current screen
	 * behind it's back.  This stupidity is because curses doesn't have
	 * a way to force a line to be updated.
	 */
	getyx(curscr, oldy, oldx);
	wmove(curscr, SCREENSIZE(curf), 0);
	for (cnt = COLS; --cnt;)
		waddch(curscr, 'x');
	wmove(curscr, oldy, oldx);
#endif

	/*
	 * Wrote a carriage return; repaint everything.  Since we waited
	 * in the v_ex() routine, no need to schedule an erase for later.
	 *
	 * If we haven't waited for the user to read the information
	 * printed (or nothing was printed), schedule an erase.
	 */
	if (ex_prstate == PR_PRINTED) {
		wrefresh(curscr);
		needexerase = 0;
	} else {
		touchline(stdscr, SCREENSIZE(curf), 0, COLS - 1);
		needexerase = 1;
	}
}
