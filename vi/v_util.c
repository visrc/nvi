/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.27 1993/03/25 15:01:42 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:01:42 $";
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
v_eof(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;
{
	u_long lno;

	if (mp == NULL)
		msgq(sp, M_BELL, "Already at end-of-file.");
	else {
		lno = file_lline(sp, ep);
		if (mp->lno >= lno)
			msgq(sp, M_BELL, "Already at end-of-file.");
		else
			msgq(sp, M_BELL,
			    "Movement past the end-of-file.");
	}
}

/*
 * v_eol --
 *	Vi end-of-line error.
 */
void
v_eol(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;	
{
	size_t len;

	if (mp == NULL)
		msgq(sp, M_BELL, "Already at end-of-line.");
	else {
		if (file_gline(sp, ep, mp->lno, &len) == NULL) {
			GETLINE_ERR(sp, mp->lno);
			return;
		}
		if (mp->cno == len - 1)
			msgq(sp, M_BELL, "Already at end-of-line.");
		else
			msgq(sp, M_BELL, "Movement past the end-of-line.");
	}
}

/*
 * v_sof --
 *	Vi start-of-file error.
 */
void
v_sof(sp, mp)
	SCR *sp;
	MARK *mp;
{
	if (mp == NULL || mp->lno == 1)
		msgq(sp, M_BELL, "Already at the beginning of the file.");
	else
		msgq(sp, M_BELL, "Movement past the beginning of the file.");
}

/*
 * v_msgflush --
 *	Flush any accumulated vi messages.
 */
int
v_msgflush(sp)
	SCR *sp;
{
	MSG *mp;
	size_t len, oldy, oldx;
	int ch;
	char *p;

	/* Ring the bell. */
	if (F_ISSET(sp, S_BELLSCHED))
		bell(sp);

	/* May not be any messages. */
	if (sp->msgp == NULL || sp->msgp->flags & M_EMPTY)
		return (0);

	/* Save off cursor position. */
	getyx(stdscr, oldy, oldx);

#define	MCONTMSG	" [More ...]"

	/* Display the messages. */
	for (mp = sp->msgp, p = NULL;
	    mp != NULL && !(mp->flags & M_EMPTY); mp = mp->next) {

		p = mp->mbuf;

lcont:		/* Move to the message line and clear it. */
		MOVE(sp, SCREENSIZE(sp), 0);
		clrtoeol();

		/* Turn on standout mode if requested. */
		if (mp->flags & (M_BELL | M_ERROR))
			standout();

		/*
		 * Figure out how much to print, and print it.
		 * Adjust for the next line.
		 */
		len = sp->cols;
		if (mp->next != NULL && !(mp->next->flags & M_EMPTY))
			len -= sizeof(MCONTMSG);
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
			while (sp->special[ch = getkey(sp, 0)] != K_CR &&
			    !isspace(ch))
				bell(sp);
		}
		if (mp->len)
			goto lcont;

		refresh();
		mp->flags |= M_EMPTY;
	}

	/* Restore cursor position. */
	MOVE(sp, oldy, oldx);
	refresh();

	/* Leave the message alone until after the next keystroke. */
	F_SET(sp, S_NEEDMERASE);

	return (0);
}
