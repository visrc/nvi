/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.11 1992/10/18 13:10:05 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:10:05 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

#define	N_EMARK		0x01		/* End of replacement mark. */
#define	N_OVERWRITE	0x02		/* Overwrite characters. */
#define	N_REPLACE	0x04		/* Replace characters without limit. */

static void	ib_err __P((void));
static int	newtext
		    __P((VICMDARG *, MARK *, u_char *, size_t, MARK *, u_int));

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
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		/*
		 * Move the cursor to one column past the end of the line and
		 * repaint the screen.
		 */
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		} else if (len) {
			curf->cno = len;
			scr_cchange(curf);
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
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		} else if (len) {
			++curf->cno;
			scr_cchange(curf);
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
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		/*
		 * Move the cursor to the start of the line and repaint
		 * the screen.
		 */
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		} else if (curf->cno != 0) {
			curf->cno = 0;
			scr_cchange(curf);
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
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
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
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		if (fm->lno == 1 && file_lline(curf) == 0)
			len = 0;
		else {
			if (file_iline(curf, fm->lno, (u_char *)"", 0))
				return (1);
			if ((p = file_gline(curf, curf->lno, &len)) == NULL) {
				GETLINE_ERR(curf->lno);
				return (1);
			}
			curf->cno = 0;
			scr_cchange(curf);
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
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1;;) {
		if (curf->lno == 1 && file_lline(curf) == 0)
			len = 0;
		else {
			if (file_aline(curf, curf->lno, (u_char *)"", 0))
				return (1);
			if ((p = file_gline(curf, ++curf->lno, &len)) == NULL) {
				GETLINE_ERR(curf->lno);
				return (1);
			}
			curf->cno = 0;
			scr_cchange(curf);
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
 * v_Change -- [buffer][count]C
 *	Change line command.
 */
int
v_Change(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	u_char *p;

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
			GETLINE_ERR(tm->lno);
			return (1);
		}
		/* Insert a line while we still can... */
		if (file_iline(curf, fm->lno, (u_char *)"", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(fm, tm, 1))
			return (1);
		if ((p = file_gline(curf, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		curf->lno = fm->lno;
		curf->cno = 0;
		scr_cchange(curf);
		return (newtext(vp, NULL, p, len, rp, 0));
	}

	if ((p = file_gline(curf, tm->lno, &len)) == NULL) {
		if (file_lline(curf) != 0) {
			GETLINE_ERR(tm->lno);
			return (1);
		}
		tm->cno = 0;
	} else
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
	u_char *p;

	lmode = vp->flags & VC_LMODE;
	if (cut(VICB(vp), fm, tm, lmode))
		return (1);

	/*
	 * If the movement is off the line, delete the range, insert a new
	 * line and go into insert mode.
	 */
	if (fm->lno != tm->lno) {
		if (file_iline(curf, fm->lno, (u_char *)"", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(fm, tm, lmode))
			return (1);
		if ((p = file_gline(curf, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		curf->lno = fm->lno;
		curf->cno = 0;
		scr_ref();
		scr_cchange(curf);
		return (newtext(vp, NULL, p, len, rp, 0));
	}

	/* Otherwise, do replacement. */
	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) != 0) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		len = 0;
	}
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
	u_char *p;

	*rp = *fm;
	notfirst = 0;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--; notfirst = 1) {
		if ((p = file_gline(curf, rp->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		}
		/*
		 * Special case.  The historic vi handled [count]R badly, in
		 * that R would replace some number of characters, and then
		 * the count would append count-1 copies of the replacing chars
		 * to the replaced space.  This seems wrong, so this version
		 * counts R commands.  Move back to where the user stopped
		 * replacing after each R command.
		 */
		if (notfirst && len) {
			++rp->cno;
			curf->lno = rp->lno;
			curf->cno = rp->cno;
			scr_ref();
			scr_cchange(curf);
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
	u_char *p;

	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) != 0) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		len = 0;
	}

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
	u_char *p;
	u_int flags;
	size_t len;
	MARK *rp;
{
	TEXT *tp;
	size_t col, insert, overwrite, rcol, startcol;
	int ch, quoted, replay, rval;
	u_char *repp;

	/* Free any previous text. */
	if (ib.head) {
		freetext(ib.head);
		ib.head = NULL;
	}

	/* Save the cursor position. */
	ib.start.lno = ib.stop.lno = curf->lno;
	ib.start.cno = ib.stop.cno = curf->cno;

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
			ib.ilb[tm->cno - 1] = CHEND;
			scr_update(curf, ib.start.lno, ib.ilb, len, LINE_RESET);
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
	 * Figure out a way to keep just the input part of the TEXT around
	 * and using it?
	 */
	rcol = 0;
	repp = ib.rep;
	replay = vp->flags & VC_ISDOT;

	/* Set up parameters. */
	col = startcol = curf->cno;
	p = ib.ilb + col;

	for (quoted = 0;;) {
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
		if (quoted) {
			quoted = 0;
			--p;
			--col;
			--curf->cno;
			goto insch;
		}

		switch(special[ch]) {
		case K_ESCAPE:			/* Escape. */
			/* Set the end cursor position. */
			ib.stop.cno = curf->cno ? curf->cno - 1 : 0;

			/* If no input, return. */
			if (ib.start.lno == ib.stop.lno &&
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

			rval = file_ibresolv(curf, &ib);
			goto done;
		case K_CR:
		case K_NL:			/* New line. */
			/* Delete any remaining overwrite characters. */
			if (!(flags & N_REPLACE) && overwrite)
				bcopy(p + overwrite, p, overwrite);

			/* Move current line into the cut buffer. */
			NEWTP;
			bcopy(ib.ilb, tp->lp, col);
			tp->len = col;
			tp->next = NULL;
			TEXTAPPEND(&ib, tp);

			/* Repaint the current line if necessary. */
			if (flags & N_REPLACE || insert) {
				bcopy(p, ib.ilb, insert + overwrite);
				scr_update(curf,
				    ib.stop.lno, tp->lp, tp->len, LINE_RESET);
			}

			/* Reset the input buffer. */
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
			if (overwrite)
				--overwrite;
			else if (insert)
				bcopy(p, p + 1, insert);
insch:			*p++ = ch;
			++col;
			++curf->cno;
			break;
		}
		ib.len = col + insert + overwrite;
		scr_update(curf, ib.stop.lno, ib.ilb, ib.len,
		    special[ch] == K_NL || special[ch] == K_CR ?
		    LINE_INSERT | LINE_LOGICAL : LINE_RESET);
		scr_cchange(curf);
	}

	/*
	 * Adjust the cursor.  If an error occurred, ib_err() makes sure the
	 * cursor is rational.  Else, if the last character didn't create a
	 * new line, we're one past the last character inserted, so back up.
	 */
done:	if (rval == 1)
		ib_err();
	else {
		rp->lno = ib.stop.lno;
		rp->cno = ib.stop.cno;
	}

	/* Free up text buffers. */
	if (ib.head != NULL) {
		freetext(ib.head);
		ib.head = NULL;
		ib.stop.lno = OOBLNO;
	}
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
