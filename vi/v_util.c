/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.13 1992/12/27 19:43:38 bostic Exp $ (Berkeley) $Date: 1992/12/27 19:43:38 $";
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

	if (msgcnt == 0)
		return (0);

	getyx(stdscr, oldy, oldx);
	MOVE(SCREENSIZE(ep), 0);
	clrtoeol();
	for (cnt = 0;;) {
		standout();
		addstr(msglist[cnt]);
		free(msglist[cnt]);
		if (++cnt < msgcnt)
			addstr(" [More ...]");
		standend();
		clrtoeol();

		if (cnt >= msgcnt)
			break;

		refresh();
		while (special[ch = getkey(0)] != K_CR && !isspace(ch))
			bell();
		MOVE(SCREENSIZE(ep), 0);
	}
	MOVE(oldy, oldx);
	refresh();
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
	struct winsize win;
	char *argv[2], sbuf[100];

	if (mode != MODE_VI)
		return;

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
		curf->scr_update(curf);
		refresh();
	}
}
