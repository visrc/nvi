/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.7 1992/06/05 11:05:30 bostic Exp $ (Berkeley) $Date: 1992/06/05 11:05:30 $";
#endif /* not lint */

#include <sys/param.h>
#include <errno.h>
#include <curses.h>
#include <stdlib.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

#define	N_EMARK		0x01		/* End of replacement mark. */
#define	N_OVERWRITE	0x02		/* Overwrite characters. */
#define	N_REPLACE	0x04		/* Replace characters without limit. */

static void	ib_err __P((void));
static int	ib_put __P((void));
static int	newtext
		    __P((VICMDARG *, MARK *, char *, size_t, MARK *, u_int));

IB ib;

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 */
int
v_iA(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		/*
		 * Move the cursor to one column past the end of
		 * the line and repaint the screen.
		 */
		EGETLINE(p, fm->lno, len);
		curf->cno = len;
		scr_cchange();
		refresh();

		if (newtext(vp, NULL, p, len, rp, 0))
			return (1);

		if (--cnt == 0)
			return (0);
		vp->flags |= VC_ISDOT;
	}
	/* NOTREACHED */
}

/*
 * v_ia -- [count]a
 *	Append text to the cursor position.
 */
int
v_ia(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		EGETLINE(p, fm->lno, len);
		if (len) {
			++curf->cno;
			scr_cchange();
			refresh();
		}
		if (newtext(vp, NULL, p, len, rp, 0))
			return (1);

		if (--cnt == 0)
			return (0);
		vp->flags |= VC_ISDOT;
	}
	/* NOTREACHED */
}

/*
 * v_iI -- [count]I
 *	Insert text at the front of the line.
 */
int
v_iI(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		/*
		 * Move the cursor to the start of the line and
		 * repaint the screen.
		 */
		EGETLINE(p, fm->lno, len);
		if (curf->cno != 0) {
			curf->cno = 0;
			scr_cchange();
			refresh();
		}
		if (newtext(vp, NULL, p, len, rp, 0))
			return (1);

		if (--cnt == 0)
			return (0);
		vp->flags |= VC_ISDOT;
	}
	/* NOTREACHED */
}

/*
 * v_ii -- [count]i
 *	Insert text at the cursor position.
 */
int
v_ii(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		EGETLINE(p, fm->lno, len);
		if (newtext(vp, NULL, p, len, rp, 0))
			return (1);

		if (--cnt == 0)
			return (0);
		vp->flags |= VC_ISDOT;
	}
	/* NOTREACHED */
}

/*
 * v_iO -- [count]O
 *	Insert text above this line.
 */
int
v_iO(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		if (file_iline(curf, fm->lno, "", 0))
			return (1);
		EGETLINE(p, curf->lno, len);
		curf->cno = 0;

		/* XXX Scroll the screen +1. */
		scr_lchange(curf->lno, p, len);
		scr_cchange();
		refresh();
		if (newtext(vp, NULL, p, len, rp, 0))
			return (1);

		if (--cnt == 0)
			return (0);
		vp->flags |= VC_ISDOT;
	}
	/* NOTREACHED */
}

/*
 * v_io -- [count]o
 *	Insert text after this line.
 */
int
v_io(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		if (file_aline(curf, fm->lno, "", 0))
			return (1);
		EGETLINE(p, ++curf->lno, len);
		curf->cno = 0;

		/* XXX Scroll the screen +1. */
		scr_lchange(curf->lno, p, len);
		scr_cchange();
		refresh();
		if (newtext(vp, NULL, p, len, rp, 0))
			return (1);

		if (--cnt == 0)
			return (0);
		vp->flags |= VC_ISDOT;
	}
	/* NOTREACHED */
}

/*
 * v_Change -- [buffer][count]C
 *	Change line command.
 */
int
v_Change(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	char *p;

	if (cut(VICB(vp), fm, tm, 1))
		return (1);

	/*
	 * There are two cases -- if a count is supplied, we do a line
	 * mode change where we delete the lines and then insert text
	 * into a new line.  Otherwise, we replace the current line.
	 */
	tm->lno = fm->lno + (vp->flags & VC_C1SET ? vp->count - 1 : 0);
	if (fm->lno != tm->lno) {
		if (file_gline(curf, tm->lno, NULL) == NULL) {
			v_eof(fm);
			return (1);
		}
		/* Insert a line while we still can... */
		if (file_iline(curf, fm->lno, "", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(fm, tm, 1))
			return (1);
		EGETLINE(p, --fm->lno, len);
		curf->lno = fm->lno;
		curf->cno = 0;
		scr_ref();
		scr_cchange();
		refresh();
		return (newtext(vp, NULL, p, len, rp, 0));
	}

	EGETLINE(p, tm->lno, len);
	tm->cno = len;
	return (newtext(vp, tm, p, len, rp, N_EMARK | N_OVERWRITE));
}

/*
 * v_change -- [buffer][count]c[count]motion
 *	Change command.
 */
int
v_change(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int lmode;
	char *p;

	lmode = vp->flags & VC_LMODE;
	if (cut(VICB(vp), fm, tm, lmode))
		return (1);

	/*
	 * If the movement is off the line, delete the range, insert a new
	 * line and go into insert mode.
	 */
	if (fm->lno != tm->lno) {
		if (file_iline(curf, fm->lno, "", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(fm, tm, lmode))
			return (1);
		EGETLINE(p, --fm->lno, len);
		curf->lno = fm->lno;
		curf->cno = 0;
		scr_ref();
		scr_cchange();
		refresh();
		return (newtext(vp, NULL, p, len, rp, 0));
	}

	/* Otherwise, do replacement. */
	EGETLINE(p, fm->lno, len);
	return (newtext(vp, tm, p, len, rp, N_EMARK | N_OVERWRITE));
}

/*
 * v_Replace -- [count]R
 *	Overwrite multiple characters.
 */
int
v_Replace(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	int notfirst;
	char *p;

	*rp = *fm;
	notfirst = 0;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--; notfirst = 1) {
		EGETLINE(p, rp->lno, len);
		/*
		 * Special case.  The historic vi handled [count]R oddly, in
		 * that the R would replace some number of characters, and then
		 * the count would append count-1 copies of replacing chars to
		 * the replaced space.  This seems wrong so this version counts
		 * R commands.  Move back to where the user quit replacing after
		 * each R command.
		 */
		if (notfirst && len) {
			++rp->cno;
			curf->lno = rp->lno;
			curf->cno = rp->cno;
			scr_ref();
			scr_cchange();
			refresh();
			vp->flags |= VC_ISDOT;
		}
		tm->lno = rp->lno;
		tm->cno = len ? len : 0;
		if (newtext(vp, tm, p, len, rp, N_OVERWRITE | N_REPLACE))
			return (1);
	}
	return (0);
}

/*
 * v_subst -- [buffer][count]s
 *	Substitute characters.
 */
int
v_subst(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	char *p;

	EGETLINE(p, fm->lno, len);

	tm->lno = fm->lno;
	tm->cno = fm->cno + (vp->flags & VC_C1SET ? vp->count : 1);
	if (tm->cno > len)
		tm->cno = len;

	if (cut(VICB(vp), fm, tm, 0))
		return (1);

	return (newtext(vp, tm, p, len, rp, N_EMARK | N_OVERWRITE));
}

#define	NEWTP {								\
	if ((tp = malloc(sizeof(TEXT))) == NULL ||			\
	    (tp->lp = malloc(col)) == NULL) {				\
		if (tp != NULL)						\
			free(tp);					\
		bell();							\
		msg("Error: %s.", strerror(errno));			\
		rval = 1;						\
		goto done;						\
	}								\
}

/*
 * newtext --
 *	Read in text from the user.
 */
static int
newtext(vp, tm, p, len, rp, flags)
	VICMDARG *vp;
	MARK *tm;
	char *p;
	u_int flags;
	size_t len;
	MARK *rp;
{
	TEXT *tp;
	size_t col, insert, overwrite, rcol, startcol;
	int ch, quoted, replay, rval;
	char *repp;

	/* Free any previous text. */
	if (ib.head) {
		freetext(ib.head);
		ib.head = NULL;
	}

	/* Save the cursor position. */
	ib.start.lno = ib.stop.lno = curf->lno;
	ib.start.lno = ib.stop.lno = curf->cno;

	/* Copy the current line for editing. */
	if (len) {
		if (binc(&ib.ilb, &ib.ilblen, len)) {
			rval = 1;
			goto done;
		}
		bcopy(p, ib.ilb, len);
		if (flags & N_OVERWRITE) {
			overwrite = tm->cno - curf->cno;
			insert = len - tm->cno;
		} else {
			overwrite = 0;
			insert = len - curf->cno;
		}
		if (flags & N_EMARK) {
			ib.ilb[tm->cno - 1] = '$';
			scr_lchange(ib.start.lno, ib.ilb, len);
			refresh();
		}
	} else
		insert = overwrite = 0;
		
	/*
	 * Set up the dot command.  Dot commands are done by saving the
	 * actual characters and replaying the input.
	 *
	 * XXX
	 * This is not quite right; we should swallow backspaces and such
	 * so that we don't repeat errors on subsequent dot operations.
	 * Should figure out a way to keep just the input part of the TEXT
	 * around and using it.
	 */
	rcol = 0;
	repp = ib.rep;
	replay = vp->flags & VC_ISDOT;

	/* Set up parameters. */
	quoted = 0;
	col = startcol = curf->cno;
	p = ib.ilb + col;

	for (;;) {
		if (col + insert >= ib.ilblen) {
			if (binc(&ib.ilb, &ib.ilblen, 0)) {
				rval = 1;
				goto done;
			}
			p = ib.ilb + col;
		}

		if (replay)
			ch = *repp++;
		else {
			if (rcol >= ib.replen) {
				if (binc(&ib.rep, &ib.replen, 0)) {
					rval = 1;
					goto done;
				}
				repp = ib.rep + rcol;
			}
			*repp++ = ch = getkey(0);
			++rcol;
		}
		if (quoted)
			goto insch;

		switch(special[ch]) {
		case K_ESCAPE:			/* Escape. */
			/* Set the end cursor position. */
			ib.stop.lno = curf->lno;
			ib.stop.cno = curf->cno;

			/* If no input, return. */
			if (ib.start.lno == ib.start.lno &&
			    ib.start.cno == ib.stop.cno) {
				rval = 0;
				goto done;
			}

			/* Delete any remaining overwrite characters. */
			if (!(flags & N_REPLACE) && overwrite) {
				bcopy(p + overwrite, p, overwrite);
				overwrite = 0;
			}

			/* Add in insert characters. */
			col += insert + overwrite;

			/* Copy the line into place. */
			NEWTP;
			bcopy(ib.ilb, tp->lp, col);
			tp->len = col;
			tp->next = NULL;
			TEXTAPPEND(&ib, tp);

			rval = ib_put();
			goto done;
		case K_CR:
		case K_NL:			/* New line. */
			/* Delete any remaining overwrite characters. */
			if (!(flags & N_REPLACE) && overwrite)
				bcopy(p + overwrite, p, overwrite);

			NEWTP;
			bcopy(ib.ilb, tp->lp, col);
			tp->len = col;
			tp->next = NULL;
			TEXTAPPEND(&ib, tp);

			/*
			 * Reset the input buffer, and repaint the current
			 * line if necessary.
			 */
			if (flags & N_REPLACE || insert) {
				bcopy(p, ib.ilb, insert + overwrite);
				scr_lchange(ib.stop.lno, tp->lp, tp->len);
			}
			p = ib.ilb;
			col = startcol = 0;

			/* Increment line count. */
			++ib.stop.lno;

			/* Reset the cursor. */
			++curf->lno;
			curf->cno = 0;
			break;
		case K_VERASE:			/* Erase the last character. */
			/*
			 * XXX
			 * Should be able to backspace over lines.
			 */
			if (col == startcol)
				bell();
			else {
				++overwrite;
				--p;
				--col;
				--curf->cno;
			}
			break;
		case K_VWERASE:			/* Erase the last word. */
			/*
			 * XXX
			 * Later.
			 */
			bell();
			break;
		case K_VKILL:			/* Restart this line. */
			col = curf->cno = startcol;
			p = ib.ilb + col;
			break;
		case K_VLNEXT:			/* Quote the next character. */
			quoted = 1;
			ch = '^';
			/* FALLTHROUGH */
		case 0:				/* Insert the character. */
insch:			if (overwrite)
				--overwrite;
			else if (insert)
				bcopy(p, p + 1, insert);
			*p++ = ch;
			++col;
			++curf->cno;
			break;
		}
		scr_cchange();
		scr_lchange(ib.stop.lno, ib.ilb, col + insert + overwrite);
		refresh();
	}

	/*
	 * Adjust the cursor.  If an error occurred, ib_err() makes sure that
	 * the cursor is rational.  Otherwise, if any data was input and the
	 * last character wasn't a CR or NL, we're one past the last character
	 * inserted, so back up one.
	 */
done:	if (rval == 1)
		ib_err();
	else {
		rp->lno = ib.stop.lno;
		rp->cno = ib.stop.cno > 0 ? ib.stop.cno - 1 : 0;
	}

	/* Free up text buffers. */
	if (ib.head != NULL) {
		freetext(ib.head);
		ib.head = NULL;
		ib.stop.lno = OOBLNO;
	}
	scr_ref();			/* XXX */
	return (rval);
}

/*
 * ib_err --
 *	Handle an error during input processing.
 */
static void
ib_err()
{
	MARK m;
	size_t len;

	/*
	 * The problem with input processing is that we're at an indeterminate
	 * cursor position since we may have actually lost some input due to
	 * a malloc error.  So, try to go back to the place from which the
	 * cursor started, but it may no longer be available.
	 */
	for (m = ib.start;
	    file_gline(curf, m.lno, &len) == NULL && --m.lno > 0;);
	if (m.lno == 0)
		m.cno = 0;
	else if (m.cno >= len)
		m.cno = len ? len - 1 : 0;

	curf->lno = m.lno;
	curf->cno = m.cno;
}

/*
 * ib_put --
 *	Insert an ib structure into the file.
 */
static int
ib_put()
{
	register TEXT *tp;
	register recno_t lno;

	/* Replace the original line. */
	tp = ib.head;
	lno = ib.start.lno;
	if (file_sline(curf, lno, tp->lp, tp->len))
		return (1);

	/* Add the new lines into the file. */
	while (tp = tp->next)
		if (file_aline(curf, lno++, tp->lp, tp->len))
			return (1);
	/* Shift any marks. */
	mark_insert(&ib.start, &ib.stop);
	return (0);
}
