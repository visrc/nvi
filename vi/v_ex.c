/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.49 1993/03/26 13:40:24 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:40:24 $";
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static int	vex_leave __P((SCR *, EXF *));
static void	vex_msg __P((SCR *, u_int, const char *, ...));
static int	vex_scroll __P((SCR *, int, int, int *));
static void	vex_start __P((SCR *));

/*
 * v_ex --
 *	Execute strings of ex commands.
 */
int
v_ex(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_int flags;
	int key;
	u_char *p;

	vex_start(sp);			/* Reset. */
	for (;;) {
		/* Get an ex command. */
		if (v_gb(sp, ISSET(O_PROMPT) ? ':' : 0, &p, &len, GB_BS) ||
		    p == NULL)
			break;

		if (len == 0)
			break;

		(void)ex_cstring(sp, ep, p, len, 1);

		(void)fflush(sp->stdfp);

		/* We may be changing files. */
		if (F_ISSET(sp, S_FILE_CHANGED))
			break;
		
		/* If only one line, don't wait. */
		if (sp->extotalcount <= 1) {
			F_SET(sp, S_NEEDMERASE);
			break;
		}

		/* The user may continue in ex mode by entering a ':'. */
		(void)vex_scroll(sp, 1, 1, &key);
		if (key != ':')
                        break;

		++sp->extotalcount;
		++sp->exlinecount;
	}

	/*
	 * The file may have changed, if so, the main vi loop will take care of
	 * it.  Otherwise, the only cursor modifications will be real, however,
	 * the underlying line may have changed; don't trust anything.  This
	 * section of code has been a remarkably fertile place for bugs.  The
	 * cursor is set to the first non-blank character by the main vi loop.
	 * Don't trust ANYTHING.
	 */
	if (!F_ISSET(sp, S_FILE_CHANGED)) {
		(void)vex_leave(sp, ep);

		rp->lno = ep->lno;
		F_SET(sp, S_CUR_INVALID);

		if (file_gline(sp, ep, ep->lno, &len) == NULL &&
		    file_lline(sp, ep) != 0) {
			GETLINE_ERR(sp, ep->lno);
			return (1);
		}
	}
	return (0);
}
		
/*
 * vex_start --
 *	Vi calls ex.
 */
static void
vex_start(sp)
	SCR *sp;
{
	sp->exlinecount = sp->extotalcount = 0;
}

/*
 * vex_leave --
 *	Ex returns to vi.
 */
static int
vex_leave(sp, ep)
	SCR *sp;
	EXF *ep;
{
	size_t loff;

	if (sp->extotalcount <= 1)	/* Don't erase if a single line. */
		return (0);

	/* Repaint if at least half the screen trashed. */
	if (sp->extotalcount >= SCREENSIZE(sp) / 2) { 
		F_SET(sp, S_REDRAW);
		return (0);
	}

	for (loff = TEXTSIZE(sp); sp->extotalcount--; --loff)
		if (scr_refresh(sp, ep, loff))
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
	SCR *sp;
	size_t new_lcontinue;
	int len, rlen;
	const char *p;

	new_lcontinue = 0;		/* In case of a write of 0. */
	p = line;

	rlen = llen;
	for (sp = cookie; llen;) {
		/* Get the next line. */
		if ((p = memchr(line, '\n', llen)) == NULL)
			len = llen;
		else
			len = p - line;

		/*
		 * The max is sp->cols characters, and we may
		 * have already written part of the line.
		 */
		if (len + sp->exlcontinue > sp->cols)
			len = sp->cols - sp->exlcontinue;

		/*
		 * If not a continuation line, and not the first line output,
		 * move the screen up.  Otherwise, move to the continuation
		 * point.
		 */
		if (sp->exlcontinue == 0) {
			if (sp->extotalcount != 0)
				(void)vex_scroll(sp, 0, 0, NULL);
			MOVE(sp, SCREENSIZE(sp), 0);
			++sp->extotalcount;
			++sp->exlinecount;
		} else
			MOVE(sp, SCREENSIZE(sp), sp->exlcontinue);

		/* Display the line. */
		if (len)
			addnstr(line, len);

		/* Clear to EOL. */
		if (len + sp->exlcontinue < sp->cols)
			clrtoeol();

		/* Set up exlcontinue. */
		new_lcontinue = len + sp->exlcontinue;
		sp->exlcontinue = 0;

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
		sp->exlcontinue = new_lcontinue;
	return (rlen);
}

static int
vex_scroll(sp, mustwait, colon_ok, chp)
	SCR *sp;
	int mustwait, colon_ok, *chp;
{
	int ch;

	/*
	 * Scroll the screen.  Instead of scrolling the entire screen, delete
	 * the line above the first line output so preserve the maximum amount
	 * of the screen.
	 */
	if (sp->extotalcount >= SCREENSIZE(sp)) {
		MOVE(sp, 0, 0);
	} else
		MOVE(sp, SCREENSIZE(sp) - sp->extotalcount, 0);
	deleteln();

	/* If just displayed a full screen, wait. */
	if (mustwait || sp->exlinecount == SCREENSIZE(sp)) {
		MOVE(sp, SCREENSIZE(sp), 0);
		addnstr(CONTMSG, (int)sizeof(CONTMSG) - 1);
		clrtoeol();
		refresh();
		while (sp->special[ch = getkey(sp, 0)] != K_CR &&
		    !isspace(ch) && (!colon_ok || ch != ':'))
			bell(sp);
		if (chp != NULL)
			*chp = ch;
		sp->exlinecount = 0;
	}
	return (0);
}
