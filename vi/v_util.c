/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.18 1993/02/19 11:25:40 bostic Exp $ (Berkeley) $Date: 1993/02/19 11:25:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

/*
 * v_eof --
 *	Vi end-of-file error.
 */
void
v_eof(ep, mp)
	EXF *ep;
	MARK *mp;
{
	u_long lno;

	if (mp == NULL)
		msg(ep, M_BELL, "Already at end-of-file.");
	else {
		lno = file_lline(ep);
		if (mp->lno >= lno)
			msg(ep, M_BELL, "Already at end-of-file.");
		else
			msg(ep, M_BELL,
			    "Movement past the end-of-file.");
	}
}

/*
 * v_eol --
 *	Vi end-of-line error.
 */
void
v_eol(ep, mp)
	EXF *ep;
	MARK *mp;	
{
	size_t len;
	u_char *p;

	if (mp == NULL)
		msg(ep, M_BELL, "Already at end-of-line.");
	else {
		if ((p = file_gline(ep, mp->lno, &len)) == NULL) {
			GETLINE_ERR(ep, mp->lno);
			return;
		}
		if (mp->cno == len - 1)
			msg(ep, M_BELL, "Already at end-of-line.");
		else
			msg(ep, M_BELL, "Movement past the end-of-line.");
	}
}

/*
 * v_sof --
 *	Vi start-of-file error.
 */
void
v_sof(ep, mp)
	EXF *ep;
	MARK *mp;
{
	if (mp == NULL || mp->lno == 1)
		msg(ep, M_BELL, "Already at the beginning of the file.");
	else
		msg(ep, M_BELL, "Movement past the beginning of the file.");
}

/*
 * v_msgflush --
 *	Flush any accumulated vi messages.
 */
int
v_msgflush(ep)
	EXF *ep;
{
	size_t oldy, oldx;
	register int ch, cnt;

	/* Ring the bell. */
	if (FF_ISSET(ep, F_BELLSCHED))
		bell(ep);

	/* May not be any messages. */
	if (msgcnt == 0)
		return (0);

	/* Display the messages. */
	getyx(stdscr, oldy, oldx);
	MOVE(ep, SCREENSIZE(ep), 0);
	clrtoeol();
	for (cnt = 0;;) {
		standout();
		addstr(msglist[cnt]);
		free(msglist[cnt]);

		/*
		 * XXX
		 * This may be wrong -- should figure out how much of the
		 * message can be displayed here, not in the msg() routine,
		 * and then display it on multiple lines, as necessary.
		 */
		if (++cnt < msgcnt)
			addstr(" [More ...]");
		standend();
		clrtoeol();

		if (cnt >= msgcnt)
			break;

		refresh();
		while (special[ch = getkey(ep, 0)] != K_CR && !isspace(ch))
			bell(ep);
		MOVE(ep, SCREENSIZE(ep), 0);
	}
	MOVE(ep, oldy, oldx);
	refresh();

	/* Leave the message alone until after the next keystroke. */
	FF_SET(ep, F_NEEDMERASE);

	msgcnt = 0;
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
	if (mode != MODE_VI)
		return;

	/*
	 * XXX
	 * Using the global `curf' here is completely unreasonable.  If
	 * the signal arrives while it's invalid, we're hosed.  This will
	 * change when we move to an event based mechanism.
	 */
	if (set_window_size(curf, 0))
		return;

	/* Do the resize if waiting. */
	if (FF_ISSET(curf, F_READING)) {
		scr_update(curf);
		refresh();
	}
}

/*
 * set_window_size --
 *	Set the window size, the row may be provided as an argument.
 */
int
set_window_size(ep, row)
	EXF *ep;
	u_int row;
{
	struct winsize win;
	char *argv[2], sbuf[100];

	/*
	 * Try TIOCGWINSZ.  If it fails, ignore the signal.  Otherwise,
	 * set the row/column options.  No error messages, because it's
	 * not worth making msg reentrant.
	 */
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) == -1)
		return (1);

	argv[0] = sbuf;
	argv[1] = NULL;

	(void)snprintf(sbuf, sizeof(sbuf), "ls=%u", row ? row : win.ws_row);
	if (opts_set(ep, argv))
		return (1);
	(void)snprintf(sbuf, sizeof(sbuf), "co=%u", win.ws_col);
	if (opts_set(ep, argv))
		return (1);

	/* Schedule resize. */
	FF_SET(curf, F_RESIZE);
	return (0);
}
