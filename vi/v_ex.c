/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.3 1992/04/28 13:50:38 bostic Exp $ (Berkeley) $Date: 1992/04/28 13:50:38 $";
#endif /* not lint */

#include <sys/types.h>
#include <termios.h>
#include <curses.h>
#include <unistd.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "tty.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_ex --
 *	Executes a string of ex commands.  If the ex commands output anything,
 *	it waits for a user keystroke before returning to the vi screen.  If
 *	that keystroke is another ':', it continues the ex commands.
 */
int ex_prerase;
MARK
v_ex()
{
	int flags, key;
	char *p;

	v_startex();
	for (flags = 0;; flags = GB_NLECHO) {
		if ((p = gb(ISSET(O_PROMPT) ? ':' : 0, flags)) == NULL)
			break;
		ex_prerase = strlen(p);
		if (ex_cstring(p, ex_prerase, 0) || ex_prstate < PR_PRINTED)
			break;
		(void)tputs(SO, 0, __putchar);
		(void)printf("Enter key to continue: ");
		(void)tputs(SE, 0, __putchar);
		(void)fflush(stdout);
		if ((key = getkey(0)) != ':')
			break;
		(void)printf("\n");
	}
	v_leaveex();
	return (cursor);
}

void
v_startex()
{
	struct termios t;

	/*
	 * Go to the ex window line and clear it.
	 *
	 * XXX
	 * This doesn't work yet; curses needs a line oriented semantic
	 * to force writing regardless of differences.
	 */
	move(LINES - 1, 0);
	clrtoeol();
	refresh();

	/* Suspend the window. */
	suspendwin();

	/* We have special needs... */
	(void)tcgetattr(STDIN_FILENO, &t);
	cfmakeraw(&t);

	/*
	 * XXX
	 * This line means that we know much too much about the tty driver.
	 * We want raw input, but we want NL -> CR mapping on output.  The
	 * only way to get that in the 4.4BSD tty driver is to have OPOST
	 * turned on and it's turned off by cfmakeraw(3).
	 */
	t.c_oflag |= ONLCR|OPOST;
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &t);

	/* Initialize the globals that let vi know what happened in ex. */
	ex_prstate = PR_NONE;
}

void
v_leaveex()
{
	/* Restart the curses window; don't repaint, lines may have changed. */
	restartwin(0);

	/*
	 * If the screen has been touched, repaint.
 	 * XXX
	 * Should check for line changes as well?
	 */
	if (ex_prstate == PR_PRINTED)
		scr_redraw(1);
}
