/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.24 1993/02/25 17:52:02 bostic Exp $ (Berkeley) $Date: 1993/02/25 17:52:02 $";
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
#include "options.h"
#include "screen.h"
#include "term.h"
#include "vcmd.h"

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

	if (mp == NULL)
		msg(ep, M_BELL, "Already at end-of-line.");
	else {
		if (file_gline(ep, mp->lno, &len) == NULL) {
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
	MSG *mp;
	size_t len, oldy, oldx;
	int ch;
	char *p;

	/* Ring the bell. */
	if (FF_ISSET(ep, F_BELLSCHED))
		bell(ep);

	/* May not be any messages. */
	if (ep->msgp == NULL || ep->msgp->flags & M_EMPTY)
		return (0);

	/* Save off cursor position. */
	getyx(stdscr, oldy, oldx);

#define	MCONTMSG	" [More ...]"

	/* Display the messages. */
	for (mp = ep->msgp, p = NULL;
	    mp != NULL && !(mp->flags & M_EMPTY); mp = mp->next) {

		p = mp->mbuf;

lcont:		/* Move to the message line and clear it. */
		MOVE(ep, SCREENSIZE(ep), 0);
		clrtoeol();

		/* Turn on standout mode if requested. */
		if (mp->flags & (M_BELL | M_ERROR))
			standout();

		/*
		 * Figure out how much to print, and print it.
		 * Adjust for the next line.
		 */
		len = SCRCOL(ep) - sizeof(MCONTMSG) - 1;
		if (mp->len < len)
			len = mp->len;
		addnstr(p, len);
		p += len;
		mp->len -= len;

		/* Turn off standout mode. */
		if (mp->flags & (M_BELL | M_ERROR))
			standend();

		/* If more, print continue message. */
		if (mp->len ||
		    mp->next != NULL && !(mp->next->flags & M_EMPTY)) {
			addnstr(MCONTMSG, sizeof(MCONTMSG) - 1);
			refresh();
			while (special[ch = getkey(ep, 0)] != K_CR &&
			    !isspace(ch))
				bell(ep);
		}
		if (mp->len)
			goto lcont;

		refresh();
		mp->flags |= M_EMPTY;
	}

	/* Restore cursor position. */
	MOVE(ep, oldy, oldx);
	refresh();

	/* Leave the message alone until after the next keystroke. */
	FF_SET(ep, F_NEEDMERASE);

	return (0);
}
