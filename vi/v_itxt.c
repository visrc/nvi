/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.24 1993/01/24 18:37:14 bostic Exp $ (Berkeley) $Date: 1993/01/24 18:37:14 $";
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

#define	N_AUTOINDENT	0x01		/* Autoindent set this line. */
#define	N_APPENDEOL	0x02		/* Appending after EOL. */
#define	N_EMARK		0x04		/* End of replacement mark. */
#define	N_OVERWRITE	0x08		/* Overwrite characters. */
#define	N_REPLACE	0x10		/* Replace; don't delete overwrite. */

#define	END_CH		'$'		/* End-of-change character. */

#define	SCREEN_UPDATE {							\
	curf->scr_update(curf);						\
	if (ISSET(O_RULER))						\
		scr_modeline(curf, 1);					\
	refresh();							\
}

static int	autoindent __P((recno_t, size_t *));
static void	ib_err __P((void));
static int	newtext __P((VICMDARG *,
		    MARK *, u_char *, size_t, MARK *, recno_t, u_int));

IB ib = { NULL, { OOBLNO, 0 }, { OOBLNO, 0 }, NULL, 0, 0, NULL, 0 };

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

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		/* Move the cursor to the end of the line. */
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		} else 
			curf->cno = len;

		/* Set flag to put an extra space at the end of the line. */
		if (newtext(vp, NULL, p, len, rp, OOBLNO, N_APPENDEOL))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
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
	int flags;
	size_t len;
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		flags = 0;
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		} else if (len) {
			if (len == curf->cno + 1) {
				flags = N_APPENDEOL;
				curf->cno = len;
			} else
				++curf->cno;
			SCREEN_UPDATE;
		}
		if (newtext(vp, NULL, p, len, rp, OOBLNO, flags))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
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

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
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
			SCREEN_UPDATE;
		}
		if (newtext(vp, NULL, p, len, rp, OOBLNO, 0))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
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

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
			if (file_lline(curf) != 0) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			len = 0;
		}
		if (newtext(vp, NULL, p, len, rp, OOBLNO, 0))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
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

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if (fm->lno == 1 && file_lline(curf) == 0) {
			p = NULL;
			len = 0;
		} else {
			p = (u_char *)"";
			len = 0;
			if (file_iline(curf, curf->lno, p, len))
				return (1);
			if ((p = file_gline(curf, curf->lno, &len)) == NULL) {
				GETLINE_ERR(curf->lno);
				return (1);
			}
			curf->cno = 0;
			SCREEN_UPDATE;
		}
		if (newtext(vp, NULL,
		    p, len, rp, curf->lno + 1, N_APPENDEOL | N_AUTOINDENT))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
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

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if (curf->lno == 1 && file_lline(curf) == 0) {
			p = NULL;
			len = 0;
		} else {
			p = (u_char *)"";
			len = 0;
			if (file_aline(curf, curf->lno, p, len))
				return (1);
			if ((p = file_gline(curf, ++curf->lno, &len)) == NULL) {
				GETLINE_ERR(curf->lno);
				return (1);
			}
			curf->cno = 0;
			SCREEN_UPDATE;
		}
		if (newtext(vp, NULL,
		    p, len, rp, curf->lno - 1, N_APPENDEOL | N_AUTOINDENT))
			return (1);

		vp->flags |= VC_ISDOT;
	}
	return (0);
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

	if (cut(curf, VICB(vp), fm, tm, 1))
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
		if (delete(curf, fm, tm, 1))
			return (1);
		if ((p = file_gline(curf, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		curf->lno = fm->lno;
		curf->cno = 0;
		SCREEN_UPDATE;
		return (newtext(vp, NULL, p, len, rp, OOBLNO, 0));
	}

	if ((p = file_gline(curf, tm->lno, &len)) == NULL) {
		if (file_lline(curf) != 0) {
			GETLINE_ERR(tm->lno);
			return (1);
		}
		tm->cno = 0;
	} else
		tm->cno = len;
	return (newtext(vp, tm, p, len, rp, OOBLNO, N_EMARK | N_OVERWRITE));
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
	if (cut(curf, VICB(vp), fm, tm, lmode))
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
		if (delete(curf, fm, tm, lmode))
			return (1);
		if ((p = file_gline(curf, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		curf->lno = fm->lno;
		curf->cno = 0;
		SCREEN_UPDATE;
		return (newtext(vp, NULL, p, len, rp, OOBLNO, 0));
	}

	/* Otherwise, do replacement. */
	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) != 0) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		len = 0;
	}
	return (newtext(vp, tm, p, len, rp, OOBLNO, N_EMARK | N_OVERWRITE));
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
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
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
		if (notfirst) {
			if (len) {
				++rp->cno;
				curf->lno = rp->lno;
				curf->cno = rp->cno;
				SCREEN_UPDATE;
				vp->flags |= VC_ISDOT;
			}
		}
		notfirst = 1;
		tm->lno = rp->lno;
		tm->cno = len ? len : 0;
		if (newtext(vp,
		    tm, p, len, rp, OOBLNO, N_OVERWRITE | N_REPLACE))
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

	if (cut(curf, VICB(vp), fm, tm, 0))
		return (1);

	return (newtext(vp, tm, p, len, rp, OOBLNO, N_EMARK | N_OVERWRITE));
}

#define	NEWTP {								\
	if ((tp = malloc(sizeof(TEXT))) == NULL ||			\
	    (tp->lp = malloc(col)) == NULL) {				\
		if (tp != NULL)						\
			free(tp);					\
		bell();							\
		msg("Error: %s.", strerror(errno));			\
		eval = 1;						\
		goto done;						\
	}								\
}

/*
 * newtext --
 *	Read in text from the user.
 */
static int
newtext(vp, tm, p, len, rp, ai_line, flags)
	VICMDARG *vp;
	MARK *tm;
	u_char *p;
	size_t len;
	MARK *rp;
	recno_t ai_line;
	u_int flags;
{
	TEXT *tp;
	size_t col, insert, overwrite, rcol, startcol;
	int carat_ch, carat_set, ch, eval, flag, in_ai, quoted, replay;
	u_char *repp;

	/* Set the initial cursor position. */
	ib.start.lno = ib.stop.lno = curf->lno;
	ib.start.cno = ib.stop.cno = curf->cno;

	/* Make sure the buffer is big enough for the special stuff. */
	if (binc(&ib.ilb, &ib.ilblen, len + 5))
		return (1);

	/*
	 * Copy the current line into the buffer for editing.  Set insert and
	 * overwrite marks.  No overwrite implies insert, and, in any case,
	 * insert after that many characters have been overwritten.  If change
	 * to a mark, flag it with END_CH.
	 */
	if (len) {
		memmove(ib.ilb, p, len);
		if (flags & N_OVERWRITE) {
			overwrite = tm->cno - curf->cno;
			insert = len - tm->cno;
		} else {
			overwrite = 0;
			insert = len - curf->cno;
		}
		if (flags & N_EMARK)
			ib.ilb[tm->cno - 1] = END_CH;
	} else
		insert = overwrite = 0;

	/*
	 * If doing auto-indent, figure out how much indentation we
	 * need, and get it filled in.  Also, set the local pointers
	 * to point into the buffer at the appropriate places, and
	 * update the cursor as necessary.
	 */
	startcol = curf->cno;
	if (flags & N_AUTOINDENT && ISSET(O_AUTOINDENT)) {
		in_ai = 1;
		if (autoindent(ai_line, &col))
			return (1);
		curf->cno = col ? col - 1 : 0;
	} else {
		in_ai = 0;
		col = startcol = curf->cno;
	}
	p = ib.ilb + col;

	/*
	 * If appending after the end-of-line, add a space into the buffer
	 * and move the cursor right.  This space is inserted, i.e. pushed
	 * along, and then deleted when the line is resolved.  NOTE: assumes
	 * that the cursor is already positioned at the end of the line.
	 */
	if (flags & N_APPENDEOL) {
		*p = '+';
		++ib.len;
		++curf->cno;
		++insert;
	}

	/* Reset the line and update the screen. */
	if (curf->scr_change(curf, ib.start.lno, LINE_RESET))
		return (1);
	SCREEN_UPDATE;
		
	/*
	 * Set up the dot command.  Dot commands are done by saving the
	 * actual characters and replaying the input.
	 *
	 * XXX
	 * This is not very clean; we should swallow backspaces and such,
	 * but it's not all that easy to do.
	 */
	rcol = 0;
	repp = ib.rep;
	replay = vp->flags & VC_ISDOT;

	for (carat_set = quoted = 0;;) {
		if (col + insert >= ib.ilblen) {
			if (binc(&ib.ilb, &ib.ilblen, 0)) {
				eval = 1;
				goto done;
			}
			p = ib.ilb + col;
		}

next_ch:	if (replay)
			ch = *repp++;
		else {
			if (rcol >= ib.replen) {
				if (binc(&ib.rep, &ib.replen, 0)) {
					eval = 1;
					goto done;
				}
				repp = ib.rep + rcol;
			}
			*repp++ = ch = getkey(GB_MAPINPUT);
			++rcol;
		}
		if (quoted) {
			--p;
			--col;
			--curf->cno;
			goto ins_qch;
		}

		switch(special[ch]) {
		case K_ESCAPE:			/* Escape. */
			/* Delete any autoindent characters. */
			if (in_ai)
				curf->cno = 0;
				
			/*
			 * Delete any extra append character, if we haven't
			 * backspaced over characters we inserted.
			 */
			if (!overwrite && flags & N_APPENDEOL) {
				--p;
				--col;
			}

			/* Set the end cursor position. */
			if (curf->cno)
				--curf->cno;
			ib.stop.cno = curf->cno;

			/* If no input, return. */
			if (ib.start.lno == ib.stop.lno &&
			    ib.start.cno == ib.stop.cno) {
				eval = 0;
				goto done;
			}

			/* Delete any remaining overwrite characters. */
			if (!(flags & N_REPLACE) && overwrite) {
				memmove(p, p + overwrite, insert);
				overwrite = 0;
			}

			/* Add in insert characters, set length. */
			ib.len = col += insert + overwrite;

			/* Update the screen. */
			curf->scr_change(curf, ib.stop.lno, LINE_RESET);
			SCREEN_UPDATE;

			/* Copy the line into place. */
			NEWTP;
			memmove(tp->lp, ib.ilb, col);
			tp->len = col;
			tp->next = NULL;
			TEXTAPPEND(&ib, tp);

			eval = file_ibresolv(curf, &ib);
			goto done;
		case K_CR:
		case K_NL:			/* New line. */
			/* Delete any autoindent characters. */
			if (in_ai) {
				col = startcol;
				curf->cno = 0;
			}

			/* Delete any remaining overwrite characters. */
			if (!(flags & N_REPLACE) && overwrite) {
				memmove(p, p + overwrite, overwrite);
				flag = 1;
				overwrite = 0;
			} else
				flag = 0;

			/* Move the current line into the cut buffer. */
			NEWTP;
			memmove(tp->lp, ib.ilb, col);
			tp->len = col;
			tp->next = NULL;
			TEXTAPPEND(&ib, tp);

			/*
			 * Increment line number, reset length, replace the
			 * current line if necessary.
			 */
			++ib.stop.lno;
			if (flags & N_REPLACE || insert || flag) {
				memmove(ib.ilb, p, insert + overwrite);
				ib.len = insert + overwrite;
			} else
				ib.len = 0;

			/* Update the current line (retrieve from TP struct). */
			if (curf->scr_change(curf,
			    ib.stop.lno - 1, LINE_RESET)) {
				eval = 1;
				goto done;
			}

			/* Reset the input buffer, handling any autoindent. */
			startcol = 0;
			if (ISSET(O_AUTOINDENT)) {
				if (autoindent(ib.stop.lno, &col)) {
					eval = 1;
					goto done;
				}
			} else
				col = 0;
			p = ib.ilb + col;
			
			/* Reset the cursor. */
			++curf->lno;
			curf->cno = 0;
			break;
		case K_CARAT:			/* Delete autoindent char. */
			/*
			 * Some utter fool decided that it would be a good idea
			 * if "^^D" deleted all of the autoindented characters.
			 * In an editor that takes single character input from
			 * the user, this is so moronic as to be unbelievable.
			 */
			if (in_ai) {
				carat_set = 1;
				goto next_ch;
			}
			goto ins_ch;
		case K_CNTRLD:			/* Delete autoindent char. */
			if (!in_ai)
				goto ins_ch;
			if (carat_set) {
				carat_set = 0;
				goto werase;
			}
			/* FALLTHROUGH */
		case K_VERASE:			/* Erase the last character. */
			/* Check for nothing to erase. */
			if (col == startcol) {
				bell();
				break;
			}
			/*
			 * Drop back one  character.  If in autoindent, delete
			 * the character -- the cwidth setting is an awful hack
			 * that forces the screen code to figure out where the
			 * cursor goes, from scratch.  This is because if we
			 * physically delete the tab, the code has no idea what
			 * character we "traversed".  Otherwise, just increment
			 * the overwrite count.
			 */
			--p;
			--col;
			--curf->cno;
			if (in_ai) {
				p[0] = p[1];
				--ib.len;
				curf->cwidth = 0;	/* XXX */
			} else
				++overwrite;
			break;
		case K_VWERASE:			/* Skip back one word. */
			/* Check for nothing to erase. */
werase:			if (col == startcol) {
				bell();
				break;
			}
			/* Skip over space characters. */
			while (col > startcol && isspace(p[-1])) {
				--p;
				--col;
				--curf->cno;

				/* If in autoindent, just lose the character. */
				if (in_ai) {
					p[0] = p[1];
					--ib.len;
				} else
					++overwrite;
			}
			if (in_ai)
				curf->cwidth = 0;	/* XXX */
			if (col == startcol)
				break;
			for (flag = inword(p[-1]); col > startcol;) {
				++overwrite;
				--p;
				--col;
				--curf->cno;
				if (flag != inword(p[-1]))
					break;
			}
			break;
		case K_VKILL:			/* Restart this line. */
			col = curf->cno = startcol;
			p = ib.ilb + col;
			break;
		case K_VLNEXT:			/* Quote the next character. */
			quoted = 2;
			ch = '^';
			/* FALLTHROUGH */
		case 0:				/* Insert the character. */
carat_lable:		if (carat_set) {
				carat_ch = ch;
				ch = '^';
			}
ins_ch:			if (overwrite)
				--overwrite;
			else if (insert)
				memmove(p + 1, p, insert);
ins_qch:		*p++ = ch;
			++col;
			++curf->cno;
			if (carat_set) {
				ch = carat_ch;
				carat_set = 0;
				goto carat_lable;
			}
			in_ai = 0;
			break;
		default:
			abort();
		}
		ib.len = col + insert + overwrite;
		curf->scr_change(curf, ib.stop.lno,
		    !quoted && (special[ch] == K_NL || special[ch] == K_CR) ?
		    LINE_INSERT : LINE_RESET);
		SCREEN_UPDATE;
		if (quoted)
			--quoted;
	}

	/*
	 * Adjust the cursor.  If an error occurred, ib_err() makes sure
	 * the cursor is rational.
	 */
done:	if (eval == 1)
		ib_err();
	else {
		rp->lno = ib.stop.lno;
		rp->cno = ib.stop.cno;
	}

	/* Free up text buffer, turn off look-aside check. */
	if (ib.head != NULL) {
		freetext(ib.head);
		ib.head = NULL;
	}
	ib.stop.lno = OOBLNO;

	return (eval);
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

static int
autoindent(lno, lenp)
	recno_t lno;
	size_t *lenp;
{
	size_t len, nlen;
	u_char *p, *t;
	
	/* Default is 0. */
	*lenp = 0;

	if ((p = t = file_gline(curf, lno, &len)) == NULL)
		return (0);
	for (; len--; ++p)
		if (!isspace(*p))
			break;

	/* No indentation. */
	if (p == t)
		return (0);

	nlen = p - t;
	if (len == 0)
		++nlen;

	/* Make sure the buffer's big enough. */
	BINC(ib.ilb, ib.ilblen, nlen);

	/* Copy the indentation into the new buffer. */
	memmove(ib.ilb, t, nlen);
	ib.len = nlen;

	/* Return the length. */
	*lenp = nlen;
	return (0);
}
