/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.9 1992/04/15 09:08:13 bostic Exp $ (Berkeley) $Date: 1992/04/15 09:08:13 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

int
ttyread(buf, len, time)
	char *buf;		/* where to store the characters */
	int len;		/* max characters to read */
	int time;		/* max tenth seconds to read */
{
	static enum { NOTSET, YES, NO } isfromtty = NOTSET;
	static fd_set rd;
	struct timeval t, *tp;

	/*
	 * Set if reading from a tty or not on the first entry.  Zero out
	 * the file descriptor array.
	 */
	if (isfromtty == NOTSET) {
		FD_ZERO(&rd);
		isfromtty = isatty(STDIN_FILENO) ? YES : NO;
	}

	/*
	 * If reading from a file or pipe, never timeout.  (This also affects
	 * the way that EOF is detected.)
	 */
	if (isfromtty == NO)
		return (read(STDIN_FILENO, buf, len));

	/* Compute the timeout value. */
	if (time) {
		t.tv_sec = time / 10;
		t.tv_usec = (time % 10) * 100000L;
		tp = &t;
	} else
		tp = NULL;

	/*
	 * Select until characters become available, and then read them.  Try
	 * to handle SIGWINCH -- if a signal arrives during the select call,
	 * adjust the O_COLUMNS and O_LINES variables, and fake a control-L.
	 */
	FD_SET(STDIN_FILENO, &rd);
	for (;;)
		switch (select(1, &rd, NULL, NULL, tp)) {
		case -1:
			/*
			 * Assume we got an EINTR because of SIGWINCH, and
			 * pretend the user hit ^L.
			 *
			 * XXX
			 * Should check EINTR, not just make the assumption.
			 */
			if (LVAL(O_LINES) != LINES || LVAL(O_COLUMNS) != COLS) {
				LVAL(O_LINES) = LINES;
				LVAL(O_COLUMNS) = COLS;
				if (mode != MODE_EX) {
					*buf = ctrl('L');
					return (1);
				}
			}
			break;
		case 0:
			/* Timeout. */
			return (0);
		default:
			/* Read it. */
			return (read(STDIN_FILENO, buf, len));
		}
}

/*
 * ex_refresh --
 *	This function calls refresh() if the option exrefresh is set.
 */
void
ex_refresh()
{
	register char *p;

	/*
	 * If this ex command wrote ANYTHING, set exwrote so vi's : command
	 * can tell that it must wait for a user keystroke before redrawing.
	 */
	for (p = kbuf; p < stdscr; p++)
		if (*p == '\n')
			exwrote = 1;

	/* Now we do the refresh thing. */
	if (ISSET(O_EXREFRESH))
		refresh();
	else
		wqrefresh();
	if (mode != MODE_VI)
		msgwaiting = 0;
}
