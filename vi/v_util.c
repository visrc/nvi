/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.25 1993/02/28 14:02:06 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:02:06 $";
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

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

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
		ep->msg(ep, M_BELL, "Already at end-of-file.");
	else {
		lno = file_lline(ep);
		if (mp->lno >= lno)
			ep->msg(ep, M_BELL, "Already at end-of-file.");
		else
			ep->msg(ep, M_BELL,
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
		ep->msg(ep, M_BELL, "Already at end-of-line.");
	else {
		if (file_gline(ep, mp->lno, &len) == NULL) {
			GETLINE_ERR(ep, mp->lno);
			return;
		}
		if (mp->cno == len - 1)
			ep->msg(ep, M_BELL, "Already at end-of-line.");
		else
			ep->msg(ep, M_BELL, "Movement past the end-of-line.");
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
		ep->msg(ep, M_BELL, "Already at the beginning of the file.");
	else
		ep->msg(ep, M_BELL, "Movement past the beginning of the file.");
}

/*
 * v_msg --
 *	Display a message.
 */
void
#ifdef __STDC__
v_msg(EXF *ep, u_int flags, const char *fmt, ...)
#else
v_msg(ep, flags, fmt, va_alist)
	EXF *ep;
	u_int flags;
        char *fmt;
        va_dcl
#endif
{
	static int reenter;
	MSG *mp;
        va_list ap;
	int len;
	char msgbuf[1024];
#ifdef __STDC__
        va_start(ap, fmt);
#else
        va_start(ap);
#endif
	/*
	 * It's possible to reenter msg when it allocates space.  We're
	 * probably dead anyway, but no reason to drop core.
	 */
	if (reenter)
		return;
	reenter = 1;

	/* Schedule a bell, if requested. */
	if (flags & (M_BELL | M_ERROR))
		FF_SET(ep, F_BELLSCHED);

	/* If extra information message, check user's wishes. */
	if (!(flags & (M_DISPLAY | M_ERROR)) && !ISSET(O_VERBOSE))
		goto done;

	/*
	 * Save the message.  If we don't have any memory, this
	 * can fail, but what's a mother to do?
	 */
	if (ep->msgp == NULL) {
		if ((ep->msgp = malloc(sizeof(MSG))) == NULL)
			goto done;
		mp = ep->msgp;
		mp->next = NULL;
		mp->mbuf = NULL;
		mp->blen = 0;
	} else {
		for (mp = ep->msgp;
		    mp->len && mp->next != NULL; mp = mp->next);
		if (mp->len) {
			if ((mp->next = malloc(sizeof(MSG))) == NULL)
				goto done;
			mp = mp->next;
			mp->next = NULL;
			mp->mbuf = NULL;
			mp->blen = 0;
		}
	}

	/* Length is the min length of the message or the buffer. */
	len = vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	if (len > sizeof(msgbuf))
		len = sizeof(msgbuf);
	else
		++len;

	/* Store the message. */
	if (len > mp->blen && binc(ep, &mp->mbuf, &mp->blen, len))
		goto done;
	memmove(mp->mbuf, msgbuf, len);
	mp->len = len;
	mp->flags = flags;

done:	reenter = 0;
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
