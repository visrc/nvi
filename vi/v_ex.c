/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.45 1993/02/28 14:37:56 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:37:56 $";
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"
#include "vcmd.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static int	vex_leave __P((EXF *));
static void	vex_msg __P((EXF *, u_int, const char *, ...));
static int	vex_scroll __P((EXF *, int, int, int *));
static void	vex_start __P((EXF *));

/*
 * v_ex --
 *	Execute strings of ex commands.
 */
int
v_ex(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_int flags;
	int key;
	u_char *p;

	vex_start(ep);			/* Reset. */
	for (;;) {
		/* Get an ex command. */
		if (v_gb(ep, ISSET(O_PROMPT) ? ':' : 0, &p, &len, GB_BS))
			break;

		if (len == 0)
			break;

		(void)ex_cstring(ep, p, len, 0);

		(void)fflush(ep->stdfp);

		/* We may be exiting, now. */
		if (FF_ISSET(ep, F_FILE_RESET))
			break;
		
		/* If only one line, don't wait. */
		if (ep->extotalcount <= 1) {
			FF_SET(ep, F_NEEDMERASE);
			break;
		}

		/* The user may continue in ex mode by entering a ':'. */
		(void)vex_scroll(ep, 1, 1, &key);
		if (key != ':')
                        break;

		++ep->extotalcount;
		++ep->exlinecount;
	}

	/*
	 * The file may have changed, if so, the main vi loop will take care of
	 * it.  Otherwise, the only cursor modifications will be real, however,
	 * the underlying line may have changed; don't trust anything.  This
	 * section of code has been a remarkably fertile place for bugs.  The
	 * cursor is set to the first non-blank character by the main vi loop.
	 * Don't trust ANYTHING.
	 */
	if (!FF_ISSET(ep, F_FILE_RESET)) {
		(void)vex_leave(ep);
		/*
		 * XXX
		 * I don't this is useful anymore...
		 *
		 * ep->olno = OOBLNO;
		 */
		rp->lno = SCRLNO(ep);
		if (file_gline(ep, SCRLNO(ep), &len) == NULL &&
		    file_lline(ep) != 0) {
			GETLINE_ERR(ep, SCRLNO(ep));
			return (1);
		}
	}
	return (0);
}
		
/*
 * vex_start --
 *	Vi calls ex.
 */
void *saveit;
static void
vex_start(ep)
	EXF *ep;
{
	ep->s_msg = ep->msg;		/* Save/set message routine. */
	ep->msg = vex_msg;

	ep->exlinecount = ep->extotalcount = 0;
}

/*
 * vex_leave --
 *	Ex returns to vi.
 */
static int
vex_leave(ep)
	EXF *ep;
{
	size_t loff;

	ep->msg = ep->s_msg;		/* Restore message routine. */

	if (ep->extotalcount <= 1)	/* Don't erase if a single line. */
		return (0);

	/* Repaint if at least half the screen trashed. */
	if (ep->extotalcount >= SCREENSIZE(ep) / 2) { 
		SF_SET(ep, S_REDRAW);
		return (0);
	}

	for (loff = TEXTSIZE(ep); ep->extotalcount--; --loff)
		if (scr_refresh(ep, loff))
			return (1);
	return (0);
}

/*
 * v_exwrite --
 *	Write out the ex messages.
 */
int
v_exwrite(cookie, line, llen)
	void *cookie;
	const char *line;
	int llen;
{
	static size_t lcont;
	EXF *ep;
	size_t new_lcont;
	int len, rlen;
	const char *p;

	new_lcont = 0;		/* In case of a write of 0. */
	p = line;

	rlen = llen;
	for (ep = cookie; llen;) {
		/* Get the next line. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/*
		 * The max is SCRCOL(ep) characters, and we may
		 * have already written part of the line.
		 */
		if (len + lcont > SCRCOL(ep))
			len = SCRCOL(ep) - lcont;

		/*
		 * If not a continuation line, and not the first line output,
		 * move the screen up.  Otherwise, move to the continuation
		 * point.
		 */
		if (lcont == 0) {
			if (ep->extotalcount != 0)
				(void)vex_scroll(ep, 0, 0, NULL);
			MOVE(ep, SCREENSIZE(ep), 0);
			++ep->extotalcount;
			++ep->exlinecount;
		} else
			MOVE(ep, SCREENSIZE(ep), lcont);

		/* Display the line. */
		if (len)
			addnstr(line, len);

		/* Clear to EOL. */
		if (len + lcont < SCRCOL(ep))
			clrtoeol();

		/* Set up lcont. */
		new_lcont = len + lcont;
		lcont = 0;

		/* Reset for the next line. */
		line += len;
		llen -= len;
		if (p != NULL) {
			++line;
			--llen;
		}
	}

	/* Set up next continuation line. */
	if (p == NULL)
		lcont = new_lcont;
	return (rlen);
}

static int
vex_scroll(ep, mustwait, colon_ok, chp)
	EXF *ep;
	int mustwait, colon_ok, *chp;
{
	int ch;

	/*
	 * Scroll the screen.  Instead of scrolling the entire screen, delete
	 * the line above the first line output so preserve the maximum amount
	 * of the screen.
	 */
	if (ep->extotalcount >= SCREENSIZE(ep)) {
		MOVE(ep, 0, 0);
	} else
		MOVE(ep, SCREENSIZE(ep) - ep->extotalcount, 0);
	deleteln();

	/* If just displayed a full screen, wait. */
	if (mustwait || ep->exlinecount == SCREENSIZE(ep)) {
		MOVE(ep, SCREENSIZE(ep), 0);
		addnstr(CONTMSG, (int)sizeof(CONTMSG) - 1);
		clrtoeol();
		refresh();
		while (special[ch = getkey(ep, 0)] != K_CR &&
		    !isspace(ch) && (!colon_ok || ch != ':'))
			bell(ep);
		if (chp != NULL)
			*chp = ch;
		ep->exlinecount = 0;
	}
	return (0);
}

/*
 * vex_msg --
 *	Display an ex message in vi.
 */
static void
#ifdef __STDC__
vex_msg(EXF *ep, u_int flags, const char *fmt, ...)
#else
vex_msg(ep, flags, fmt, va_alist)
	EXF *ep;
	u_int flags;
        char *fmt;
        va_dcl
#endif
{
        va_list ap;
	size_t len;
	char buf[1024];
#ifdef __STDC__
        va_start(ap, fmt);
#else
        va_start(ap);
#endif

	/* If extra information message, check user's wishes. */
	if (!(flags & (M_DISPLAY | M_ERROR)) && !ISSET(O_VERBOSE))
		return;

	/* Don't bother with allocation, sizeof(buf) should be big enough. */
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;
	buf[len++] = '\n';
	v_exwrite(ep, buf, len);
}
