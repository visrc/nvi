/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.35 1993/02/16 20:08:23 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:08:23 $";
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
static enum { NOTSET, NEXTLINE, THISLINE } continueline;

static int	moveup __P((EXF *, int, int, int *));
static void	v_leaveex __P((EXF *));
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

	for (flags = GB_BS;;) {
		v_startex();			/* Reset. */

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
		v_leaveex(ep);
		ep->olno = OOBLNO;
		rp->lno = ep->lno;
		if (file_gline(ep, ep->lno, &len) == NULL &&
		    file_lline(ep) != 0) {
			GETLINE_ERR(ep, ep->lno);
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
	continueline = NOTSET;
}

/*
 * v_leaveex --
 *	Ex returns to vi.
 */
static void
v_leaveex(ep)
	EXF *ep;
{
	/* Don't erase if only a single line. */
	if (extotalcount <= 1)
		return;

	/* Repaint if at least half the screen trashed. */
	if (extotalcount >= SCREENSIZE(ep) / 2) { 
		FF_SET(ep, F_REDRAW);
		return;
	}

	/* Repaint the overwritten lines. */
	do {
		--extotalcount;
		(void)ep->scr_change(ep,
		    BOTLINE(ep, ep->otop) - extotalcount, LINE_RESET);
	} while (extotalcount);
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
	int len, rlen, tlen;
	char *p;

	for (ep = cookie, rlen = llen; llen;) {
		/* Newline delimits. */
		if ((p = memchr(line, '\n', llen)) == NULL) {
			len = llen;
			lcont = len;
			continueline = NEXTLINE;
		} else
			len = p - line;

		/* Fold if past end-of-screen. */
		if (len > ep->cols) {
			continueline = NOTSET;
			len = ep->cols;
		}
		llen -= len + (p == NULL ? 0 : 1);

		if (continueline != THISLINE && extotalcount != 0)
			(void)moveup(ep, 0, 0, NULL);

		switch (continueline) {
		case NEXTLINE:
			continueline = THISLINE;
			/* FALLTHROUGH */
		case NOTSET:
			MOVE(ep, SCREENSIZE(ep), 0);
			++extotalcount;
			++exlinecount;
			tlen = len;
			break;
		case THISLINE:
			MOVE(ep, SCREENSIZE(ep), lcont);
			tlen = len + lcont;
			continueline = NOTSET;
			break;
		default:
			abort();
		}
		addnstr(line, len);

		/* GCC: tlen cannot be uninitialized here, see switch above. */
		if (tlen < ep->cols)
			clrtoeol();

		line += len + (p == NULL ? 0 : 1);
	}
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
		addnstr(CONTMSG, sizeof(CONTMSG) - 1);
		clrtoeol();
		refresh();
		while (special[ch = getkey(ep, 0)] != K_CR &&
		    !isspace(ch) && (!colon_ok || ch != ':'))
			bell(ep);
		if (chp != NULL)
			*chp = ch;
		exlinecount = 0;
	}
}
