/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.6 1992/05/07 12:48:48 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:48:48 $";
#endif /* not lint */

#include <sys/types.h>
#include <termios.h>
#include <curses.h>
#include <unistd.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_ex --
 *	Execute strings of ex commands.
 */
MARK *
v_ex()
{
	int flags, key;
	char *p;

	v_startex();
	for (flags = GB_BS;; flags |= GB_NLECHO) {
		/*
		 * Get an ex command; echo the newline on any prompts after
		 * the first.  We may have to overwrite the command later;
		 * get the length for later.
		 */
		if ((p =
		    gb(ISSET(O_PROMPT) ? ':' : 0, &ex_prerase, flags)) == NULL)
			break;

		/*
		 * Execute the command.  If the command fails, and nothing was
		 * printed, we return to vi, confident that the error messages
		 * will be displayed.  If something has been printed, we want
		 * to group the errors together with the normal output, so we
		 * supply a terminating newline if it's needed, and then display
		 * the error messages.
		 *
		 * If successful and nothing was printed, return to vi.
		 *
		 * In either case, if something was printed, wait for the user
		 * to confirm that they saw it.
		 */
		if (ex_cstring(p, ex_prerase, 0)) {
			if (ex_prstate == PR_NONE)
				break;
			if (ex_prstate == PR_STARTED)
				EX_PRNEWLINE;
			if (msgcnt)
				msg_eflush();
			else
				(void)printf("Error...\n");
		} else if (ex_prstate < PR_PRINTED)
			break;
		(void)tputs(SO, 0, __putchar);
		(void)printf("Enter key to continue: ");
		(void)tputs(SE, 0, __putchar);
		(void)fflush(stdout);

		/* The user may continue in ex mode by entering a ':'. */
		if ((key = getkey(0)) != ':')
			break;
		(void)printf("\n");
	}
	v_leaveex();
	return (&cursor);
}

static u_long oldy, oldx;

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
	getyx(stdscr, oldy, oldx);
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
	/* Restart the curses window, repainting if necessary. */
	restartwin(ex_prstate == PR_PRINTED);

	/* Return to the last cursor position. */
	move(oldy, oldx);
}
