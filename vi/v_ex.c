/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.43 1993/02/24 12:57:40 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:57:40 $";
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

static size_t exlinecount, extotalcount;

static int	moveup __P((EXF *, int, int, int *));
static int	v_leaveex __P((EXF *));
static void	v_startex __P((void));

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
	int flags, key;
	u_char *p;

	v_startex();			/* Reset. */
	for (flags = GB_BS;;) {
		/*
		 * Get an ex command; echo the newline on any prompts after
		 * the first.
		 */
		if (v_gb(ep, ISSET(O_PROMPT) ? ':' : 0, &p, &len, flags) ||
		    p == NULL)
			break;
		flags |= GB_NLECHO;

		if (len == 0)
			break;

		(void)ex_cstring(ep, p, len, 0);
		/*
		 * XXX
		 * THE UNDERLYING EXF MAY HAVE CHANGED.
		 */
		ep = curf;

		(void)fflush(ep->stdfp);
		if (extotalcount <= 1) {
			FF_SET(ep, F_NEEDMERASE);
			break;
		}

		/* The user may continue in ex mode by entering a ':'. */
		(void)moveup(ep, 1, 1, &key);
		if (key != ':')
                        break;
	}

	/*
	 * The file may have changed, if so, the main vi loop will take care of
	 * it.  Otherwise, the only cursor modifications will be real, however,
	 * the underlying line may have changed; don't trust anything.  This
	 * section of code has been a remarkably fertile place for bugs.  The
	 * cursor is set to the first non-blank character by the main vi loop.
	 * Don't trust ANYTHING.
	 */
	if (!FF_ISSET(ep, F_NEWSESSION)) {
		(void)v_leaveex(ep);
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
 * v_startex --
 *	Vi calls ex.
 */
static void
v_startex()
{
	exlinecount = extotalcount = 0;
}

/*
 * v_leaveex --
 *	Ex returns to vi.
 */
static int
v_leaveex(ep)
	EXF *ep;
{
	size_t loff;

	/* Don't erase if only a single line. */
	if (extotalcount <= 1)
		return (0);

	/* Repaint if at least half the screen trashed. */
	if (extotalcount >= SCREENSIZE(ep) / 2) { 
		SF_SET(ep, S_REDRAW);
		return (0);
	}

	for (loff = TEXTSIZE(ep); extotalcount--; --loff)
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
			if (extotalcount != 0)
				(void)moveup(ep, 0, 0, NULL);
			MOVE(ep, SCREENSIZE(ep), 0);
			++extotalcount;
			++exlinecount;
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
moveup(ep, mustwait, colon_ok, chp)
	EXF *ep;
	int mustwait, colon_ok, *chp;
{
	int ch;

	/*
	 * Scroll the screen.  Instead of scrolling the entire screen, delete
	 * the line above the first line output so preserve the maximum amount
	 * of the screen.
	 */
	if (extotalcount >= SCREENSIZE(ep)) {
		MOVE(ep, 0, 0);
	} else
		MOVE(ep, SCREENSIZE(ep) - extotalcount, 0);
	deleteln();

	/* If just displayed a full screen, wait. */
	if (mustwait || exlinecount == SCREENSIZE(ep)) {
		MOVE(ep, SCREENSIZE(ep), 0);
		addnstr(CONTMSG, (int)sizeof(CONTMSG) - 1);
		clrtoeol();
		refresh();
		while (special[ch = getkey(ep, 0)] != K_CR &&
		    !isspace(ch) && (!colon_ok || ch != ':'))
			bell(ep);
		if (chp != NULL)
			*chp = ch;
		exlinecount = 0;
	}
	return (0);
}
