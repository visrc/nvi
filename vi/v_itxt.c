/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.30 1993/02/24 13:02:59 bostic Exp $ (Berkeley) $Date: 1993/02/24 13:02:59 $";
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
#include "options.h"
#include "screen.h"
#include "term.h"
#include "vcmd.h"

#define	N_AUTOINDENT	0x01		/* Autoindent set this line. */
#define	N_APPENDEOL	0x02		/* Appending after EOL. */
#define	N_EMARK		0x04		/* End of replacement mark. */
#define	N_OVERWRITE	0x08		/* Overwrite characters. */
#define	N_REPLACE	0x10		/* Replace; don't delete overwrite. */

#define	END_CH		'$'		/* End-of-change character. */

static int	autoindent __P((EXF *, recno_t, size_t *));
static void	ib_err __P((EXF *));
static int	newtext __P((EXF *, VICMDARG *,
		    MARK *, u_char *, size_t, MARK *, recno_t, u_int));

IB ib = { NULL, { OOBLNO, 0 }, { OOBLNO, 0 }, NULL, 0, 0, NULL, 0 };

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 */
int
v_iA(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		/* Move the cursor to the end of the line. */
		if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
			if (file_lline(ep) != 0) {
				GETLINE_ERR(ep, fm->lno);
				return (1);
			}
			len = 0;
		} else 
			SCRCNO(ep) = len;

		/* Set flag to put an extra space at the end of the line. */
		if (newtext(ep, vp, NULL, p, len, rp, OOBLNO, N_APPENDEOL))
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
v_ia(ep, vp, fm, tm, rp)
	EXF *ep;
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
		if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
			if (file_lline(ep) != 0) {
				GETLINE_ERR(ep, fm->lno);
				return (1);
			}
			flags = N_APPENDEOL;
			len = 0;
		} else if (len) {
			if (len == SCRCNO(ep) + 1) {
				flags = N_APPENDEOL;
				SCRCNO(ep) = len;
			} else
				++SCRCNO(ep);
		} else
			flags = N_APPENDEOL;
		if (newtext(ep, vp, NULL, p, len, rp, OOBLNO, flags))
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
v_iI(ep, vp, fm, tm, rp)
	EXF *ep;
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
		if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
			if (file_lline(ep) != 0) {
				GETLINE_ERR(ep, fm->lno);
				return (1);
			}
			len = 0;
		} else if (SCRCNO(ep) != 0)
			SCRCNO(ep) = 0;
		if (newtext(ep, vp,
		    NULL, p, len, rp, OOBLNO, len == 0 ? N_APPENDEOL : 0))
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
v_ii(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
			if (file_lline(ep) != 0) {
				GETLINE_ERR(ep, fm->lno);
				return (1);
			}
			len = 0;
		}
		if (newtext(ep, vp,
		    NULL, p, len, rp, OOBLNO, len == 0 ? N_APPENDEOL : 0))
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
v_iO(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if (fm->lno == 1 && file_lline(ep) == 0) {
			p = NULL;
			len = 0;
		} else {
			p = (u_char *)"";
			len = 0;
			if (file_iline(ep, SCRLNO(ep), p, len))
				return (1);
			if ((p = file_gline(ep, SCRLNO(ep), &len)) == NULL) {
				GETLINE_ERR(ep, SCRLNO(ep));
				return (1);
			}
			SCRCNO(ep) = 0;
		}
		if (newtext(ep, vp, NULL,
		    p, len, rp, SCRLNO(ep) + 1, N_APPENDEOL | N_AUTOINDENT))
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
v_io(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	u_char *p;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt; --cnt) {
		if (SCRLNO(ep) == 1 && file_lline(ep) == 0) {
			p = NULL;
			len = 0;
		} else {
			p = (u_char *)"";
			len = 0;
			if (file_aline(ep, SCRLNO(ep), p, len))
				return (1);
			if ((p = file_gline(ep, ++SCRLNO(ep), &len)) == NULL) {
				GETLINE_ERR(ep, SCRLNO(ep));
				return (1);
			}
			SCRCNO(ep) = 0;
		}
		if (newtext(ep, vp, NULL,
		    p, len, rp, SCRLNO(ep) - 1, N_APPENDEOL | N_AUTOINDENT))
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
v_Change(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags;
	u_char *p;

	/*
	 * There are two cases -- if a count is supplied, we do a line
	 * mode change where we delete the lines and then insert text
	 * into a new line.  Otherwise, we replace the current line.
	 */
	tm->lno = fm->lno + (vp->flags & VC_C1SET ? vp->count - 1 : 0);
	if (fm->lno != tm->lno) {
		/* Make sure that the to line is real. */
		if (file_gline(ep, tm->lno, NULL) == NULL) {
			GETLINE_ERR(ep, tm->lno);
			return (1);
		}

		/* Cut the line. */
		if (cut(ep, VICB(vp), fm, tm, 1))
			return (1);

		/* Insert a line while we still can... */
		if (file_iline(ep, fm->lno, (u_char *)"", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(ep, fm, tm, 1))
			return (1);
		if ((p = file_gline(ep, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(ep, fm->lno);
			return (1);
		}
		SCRLNO(ep) = fm->lno;
		SCRCNO(ep) = 0;
		return (newtext(ep, vp, NULL, p, len, rp, OOBLNO, 0));
	}

	/* The line may be empty, but that's okay. */
	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) != 0) {
			GETLINE_ERR(ep, tm->lno);
			return (1);
		}
		flags = N_APPENDEOL;
		len = 0;
	} else {
		if (cut(ep, VICB(vp), fm, tm, 1))
			return (1);
		tm->cno = len;
		flags = N_EMARK | N_OVERWRITE;
	}
	return (newtext(ep, vp, fm, p, len, rp, OOBLNO, flags));
}

/*
 * v_change -- [buffer][count]c[count]motion
 *	Change command.
 */
int
v_change(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags, lmode;
	u_char *p;

	lmode = vp->flags & VC_LMODE;

	/*
	 * If the movement is off the line, delete the range, insert a new
	 * line and go into insert mode.
	 */
	if (fm->lno != tm->lno) {
		/* Cut the line. */
		if (cut(ep, VICB(vp), fm, tm, lmode))
			return (1);

		/* Insert a line while we still can... */
		if (file_iline(ep, fm->lno, (u_char *)"", 0))
			return (1);
		++fm->lno;
		++tm->lno;
		if (delete(ep, fm, tm, lmode))
			return (1);
		if ((p = file_gline(ep, --fm->lno, &len)) == NULL) {
			GETLINE_ERR(ep, fm->lno);
			return (1);
		}
		SCRLNO(ep) = fm->lno;
		SCRCNO(ep) = 0;
		return (newtext(ep, vp, NULL, p, len, rp, OOBLNO, 0));
	}

	/* Otherwise, do replacement. */
	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) != 0) {
			GETLINE_ERR(ep, fm->lno);
			return (1);
		}
		flags = N_APPENDEOL;
		len = 0;
	} else {
		/* Cut the line. */
		if (cut(ep, VICB(vp), fm, tm, lmode))
			return (1);
		flags = N_EMARK | N_OVERWRITE;
	}
	return (newtext(ep, vp, tm, p, len, rp, OOBLNO, flags));
}

/*
 * v_Replace -- [count]R
 *	Overwrite multiple characters.
 */
int
v_Replace(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	int flags, notfirst;
	u_char *p;

	*rp = *fm;
	notfirst = 0;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		if ((p = file_gline(ep, rp->lno, &len)) == NULL) {
			if (file_lline(ep) != 0) {
				GETLINE_ERR(ep, fm->lno);
				return (1);
			}
			flags = N_APPENDEOL;
			len = 0;
		} else
			flags = N_OVERWRITE | N_REPLACE;
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
			SCRLNO(ep) = rp->lno;
			SCRCNO(ep) = rp->cno;
			vp->flags |= VC_ISDOT;
		}
		notfirst = 1;
		tm->lno = rp->lno;
		tm->cno = len ? len : 0;
		if (newtext(ep, vp, tm, p, len, rp, OOBLNO, flags))
			return (1);
	}
	return (0);
}

/*
 * v_subst -- [buffer][count]s
 *	Substitute characters.
 */
int
v_subst(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	int flags;
	u_char *p;

	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) != 0) {
			GETLINE_ERR(ep, fm->lno);
			return (1);
		}
		len = 0;
		flags = N_APPENDEOL;
	} else
		flags = N_EMARK | N_OVERWRITE;

	tm->lno = fm->lno;
	tm->cno = fm->cno + (vp->flags & VC_C1SET ? vp->count : 1);
	if (tm->cno > len)
		tm->cno = len;

	if (p != NULL && cut(ep, VICB(vp), fm, tm, 0))
		return (1);

	return (newtext(ep, vp, tm, p, len, rp, OOBLNO, flags));
}

/* Allocate a new TEXT structure. */
#define	NEWTP(ep) {							\
	if ((tp = malloc(sizeof(TEXT))) == NULL ||			\
	    (tp->lp = malloc(ib.len)) == NULL) {			\
		if (tp != NULL)						\
			free(tp);					\
		msg(ep, M_ERROR, "Error: %s.", strerror(errno));	\
		eval = 1;						\
		goto done;						\
	}								\
	memmove(tp->lp, ib.ilb, ib.len);				\
	tp->len = ib.len;						\
	tp->next = NULL;						\
	TEXTAPPEND(&ib, tp);						\
}

#define	SCREEN_UPDATE(ep) {						\
	if (scr_update(ep)) {						\
		eval = 1;						\
		goto done;						\
	}								\
	if (ISSET(O_RULER))						\
		scr_modeline(ep, 1);					\
	refresh();							\
}

/*
 * newtext --
 *	Read in text from the user.
 *
 * XXX
 * Make quoted an enum.
 */
static int
newtext(ep, vp, tm, p, len, rp, ai_line, flags)
	EXF *ep;
	VICMDARG *vp;
	MARK *tm;		/* To MARK. */
	u_char *p;		/* Input line, then input buffer pointer. */
	size_t len;		/* Input line length. */
	MARK *rp;		/* Return MARK. */
	recno_t ai_line;	/* Line number to use for autoindent count. */
	u_int flags;		/* N_stuff. */
{
				/* State of the "[^0]^D" sequences. */
	enum { C_CARATSET, C_NOINC, C_NOTSET, C_ZEROSET } carat_st;
	TEXT *tp;		/* Input text structure. */
	size_t col;		/* The current column in the buffer. */
	size_t insert;		/* The number of bytes to push. */
	size_t overwrite;	/* The number of bytes to overwrite. */
	size_t rcol;		/* Insert offset in the replay buffer. */
	size_t startcol;	/* Starting column in the buffer. */
	int carat_ch;		/* Character after the ^ character. */
	int ch;			/* Input character. */
	int eval;		/* Routine return value. */
	int in_ai;		/* If no characters after autoindented ones. */
	int quoted;		/* If next character is quoted. */
	int replay;		/* If replaying a set of input. */
	int tmp;
	u_char *repp;		/* Replay buffer. */

	/* Set the initial cursor position. */
	ib.start.lno = ib.stop.lno = SCRLNO(ep);
	ib.start.cno = ib.stop.cno = SCRCNO(ep);

	/* Make sure the buffer is big enough even if the line is empty. */
	if (binc(ep, &ib.ilb, &ib.ilblen, len + 5))
		return (1);

	/*
	 * If there's a line, copy it into the buffer for editing.
	 *
	 * Set insert and overwrite counts.  If nothing is being overwritten,
	 * we assume we're doing insertion.  Even if overwriting, do insertion
	 * once the specified characters are overwritten.  If change is to a
	 * mark, flag it with END_CH.
	 */
	if (len) {
		memmove(ib.ilb, p, len);
		ib.len = len;
		if (flags & N_OVERWRITE) {
			overwrite = tm->cno - SCRCNO(ep);
			insert = len - tm->cno;
		} else {
			overwrite = 0;
			insert = len - SCRCNO(ep);
		}
		if (flags & N_EMARK)
			ib.ilb[tm->cno - 1] = END_CH;
	} else {
		ib.len = 0;
		insert = overwrite = 0;
	}

	/*
	 * Many of the special cases in this routine are to handle autoindent
	 * support.  Some utter fool decided that it would be a good idea if
	 * "^^D" and "0^D" deleted all of the autoindented characters.  In an
	 * editor that takes single character input from the user, this is so
	 * stupid as to be unbelievable.  Note also that "^^D" resets the next
	 * lines' autoindent, but "0^D" doesn't.
	 *
	 * We assume that auto-indent only happens on empty lines, so insert
	 * and overwrite should already be zero.  If doing auto-indent, figure
	 * out how much indentation we need, and get it filled in.  Also, set
	 * the local pointers to point into the buffer at the right places, and
	 * update the screen cursor as necessary.
	 */
	if (flags & N_AUTOINDENT && ISSET(O_AUTOINDENT)) {
		if (autoindent(ep, ai_line, &col))
			return (1);
		in_ai = 1;
		startcol = 0;
		SCRCNO(ep) = col ? col - 1 : 0;
	} else {
		in_ai = 0;
		col = startcol = SCRCNO(ep);
	}

	/* Point to the first empty slot in which to insert a character. */
	p = ib.ilb + col;

	/*
	 * If appending after the end-of-line, add a space into the buffer
	 * and move the cursor right.  This space is inserted, i.e. pushed
	 * along, and then deleted when the line is resolved.  Assumes that
	 * the cursor is already positioned at the end of the line.
	 */
	if (flags & N_APPENDEOL) {
		*p = '+';
		SCRCNO(ep) = ib.len;
		++ib.len;
		++insert;
	}

	/* Reset the line and update the screen. */
	if (scr_change(ep, ib.start.lno, LINE_RESET))
		return (1);
	SCREEN_UPDATE(ep);
		
	/*
	 * Set up the dot command.  Dot commands are done by saving the
	 * actual characters and replaying the input.
	 *
	 * XXX
	 * This is not as clean as it should be.  It would be nice if we
	 * could swallow backspaces and such, but it's not all that easy
	 * to do.  Another possibility would be to recognize full line
	 * insertions, which could be performed quickly, without replay.
	 */
	rcol = 0;
	repp = ib.rep;
	replay = vp->flags & VC_ISDOT;

	for (carat_st = C_NOTSET, eval = quoted = 0;;) {
next_ch:	if (replay)
			ch = *repp++;
		else {
			/*
			 * Check if the character fits into the input and
			 * replay buffers; allocate space as necesssary.
			 */
			if (col + insert >= ib.ilblen) {
				if (binc(ep, &ib.ilb, &ib.ilblen, 0)) {
					eval = 1;
					goto done;
				}
				p = ib.ilb + col;
			}

			if (rcol >= ib.replen) {
				if (binc(ep, &ib.rep, &ib.replen, 0)) {
					eval = 1;
					goto done;
				}
				repp = ib.rep + rcol;
			}
			/* Store the character into the replay buffer. */
			*repp++ = ch = getkey(ep, GB_BEAUTIFY | GB_MAPINPUT);
			++rcol;
		}

		/*
		 * If the character is quoted, delete the last character
		 * (the literal mark), and insert the character without
		 * further ado.
		 */
		if (quoted) {
			--p;
			--col;
			--SCRCNO(ep);
			goto ins_qch;
		}

		switch(special[ch]) {
		case K_ESCAPE:				/* Escape. */
			/*
			 * If the user hasn't inserted any characters,
			 * delete the autoindent characters.
			 *
			 * If we haven't been backspacing over characters
			 * already, delete any appended space character.
			 */
			if (in_ai) {
				p = ib.ilb;
				ib.len = 0;
				SF_SET(ep, S_CHARDELETED);
			} else if (flags & N_APPENDEOL) {
				--p;
				--ib.len;
				--insert;
				SF_SET(ep, S_CHARDELETED);
			}

			/*
			 * The cursor normally follows the last inserted
			 * character.  Reset the return cursor position
			 * to rest on the last inserted character.
			 */
			if (SCRCNO(ep))
				--SCRCNO(ep);
			ib.stop.cno = SCRCNO(ep);

			/* If no input, just return. */
			if (ib.start.lno == ib.stop.lno &&
			    ib.start.cno == ib.stop.cno)
				goto done;

			/*
			 * The "R" command doesn't delete the characters
			 * that it could have overwritten.  Other input
			 * modes do.
			 *
			 * Delete any remaining "overwrite" characters by
			 * copying the "insert" characters over them.
			 */
			if (!(flags & N_REPLACE) && overwrite && insert)
				memmove(p, p + overwrite, insert);
			overwrite = 0;

			/* Update the screen. */
			scr_change(ep, ib.stop.lno, LINE_RESET);
			SCREEN_UPDATE(ep);

			/* Append the line into the text structure. */
			NEWTP(ep);

			/* Resolve the input lines into the file. */
			eval = file_ibresolv(ep, ib.start.lno);
			goto done;
		case K_CR:
		case K_NL:				/* New line. */
			/*
			 * If the user hasn't inserted any characters,
			 * delete the autoindent characters.
			 */
			if (in_ai) {
				col = 0;
				SF_SET(ep, S_CHARDELETED);
			}

			/* Ignore the rest of the line. */
			ib.len = col;

			/*
			 * The "R" command doesn't delete the characters
			 * that it could have overwritten.  Other input
			 * modes do.
			 */
			if (flags & N_REPLACE)
				insert += overwrite;
			else
				p += overwrite;
			overwrite = 0;

			/* Append the line into the text structure. */
			NEWTP(ep);

			/*
			 * Update the screen.  Update the current line number
			 * so the line is retrieved from the TEXT structure.
			 */
			++ib.stop.lno;
			if (scr_change(ep, ib.stop.lno - 1, LINE_RESET)) {
				eval = 1;
				goto done;
			}

			/*
			 * Copy the remaining characters to the start of the
			 * input buffer, reset the buffer.
			 */
			if (insert) {
				memmove(ib.ilb, p, insert);
				ib.len = insert;
			} else
				ib.len = 0;

			/* Reset the autoindent if necessary. */
			if (carat_st == C_NOINC)
				carat_st = C_NOTSET;
			else if (!in_ai)
				ai_line = ib.stop.lno - 1;

			/* Reset the input buffer, adding any autoindent. */
			startcol = 0;
			if (ISSET(O_AUTOINDENT)) {
				if (autoindent(ep, ai_line, &col)) {
					eval = 1;
					goto done;
				}
				in_ai = 1;
			} else
				col = 0;
			ib.len += col;
			p = ib.ilb + col;
			
			/* Reset the cursor. */
			SCRLNO(ep) = ib.stop.lno;
			SCRCNO(ep) = col;
			break;
		case K_CARAT:			/* Delete autoindent chars. */
			if (in_ai) {
				carat_st = C_CARATSET;
				goto next_ch;
			}
			goto ins_ch;
		case K_ZERO:			/* Delete autoindent chars. */
			if (in_ai) {
				carat_st = C_ZEROSET;
				goto next_ch;
			}
			goto ins_ch;
		case K_CNTRLD:			/* Delete autoindent char. */
			if (!in_ai)
				goto ins_ch;
			switch(carat_st) {
			case C_CARATSET:
				carat_st = C_NOTSET;
				goto werase;
			case C_ZEROSET:
				carat_st = C_NOINC;
				goto werase;
			case C_NOTSET:
				break;
			default:
				abort();
			}
			/* FALLTHROUGH */
		case K_VERASE:			/* Erase the last character. */
			/* Check for nothing to erase. */
			if (col == startcol) {
				bell(ep);
				break;
			}
			/*
			 * Drop back one character.  If in autoindent, delete
			 * the character, otherwise, increment the overwrite
			 * count.
			 */
			--p;
			--col;
			--SCRCNO(ep);
			if (in_ai) {
				p[0] = p[1];
				--ib.len;
				SF_SET(ep, S_CHARDELETED);
			} else
				++overwrite;
			break;
		case K_VWERASE:			/* Skip back one word. */
			/* Check for nothing to erase. */
werase:			if (col == startcol) {
				bell(ep);
				break;
			}
			/* Skip over space characters. */
			while (col > startcol && isspace(p[-1])) {
				--p;
				--col;
				--SCRCNO(ep);

				/* If in autoindent, just lose the character. */
				if (in_ai) {
					p[0] = p[1];
					--ib.len;
				} else
					++overwrite;
			}
			if (in_ai)
				SF_SET(ep, S_CHARDELETED);
			if (col == startcol)
				break;
			for (tmp = inword(p[-1]); col > startcol;) {
				++overwrite;
				--p;
				--col;
				--SCRCNO(ep);
				if (tmp != inword(p[-1]))
					break;
			}
			break;
		case K_VKILL:			/* Restart this line. */
			col = SCRCNO(ep) = startcol;
			p = ib.ilb + col;
			break;
		case K_VLNEXT:			/* Quote the next character. */
			quoted = 2;
			ch = '^';
			/* FALLTHROUGH */
		default:			/* Insert the character. */
carat_lable:		if (carat_st == C_ZEROSET || carat_st == C_CARATSET) {
				carat_ch = ch;
				ch = '^';
			}
ins_ch:			if (overwrite) {
				--overwrite;
				SF_SET(ep, S_CHARDELETED);
			} else if (insert)
				memmove(p + 1, p, insert);
ins_qch:		*p++ = ch;
			++col;
			++SCRCNO(ep);
			if (carat_st == C_ZEROSET || carat_st == C_CARATSET) {
				ch = carat_ch;
				carat_st = C_NOTSET;
				goto carat_lable;
			}
			in_ai = 0;
			break;
		}
		ib.len = col + insert + overwrite;
		scr_change(ep, ib.stop.lno,
		    !quoted && (special[ch] == K_NL || special[ch] == K_CR) ?
		    LINE_INSERT : LINE_RESET);
		SCREEN_UPDATE(ep);
		if (quoted)
			--quoted;
	}

	/*
	 * Adjust the cursor.  If an error occurred, ib_err() makes sure
	 * the cursor is rational.
	 */
done:	if (eval == 1)
		ib_err(ep);
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
ib_err(ep)
	EXF *ep;
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
	    file_gline(ep, m.lno, &len) == NULL && --m.lno > 0;);
	if (m.lno == 0)
		m.cno = 0;
	else if (m.cno >= len)
		m.cno = len ? len - 1 : 0;

	SCRLNO(ep) = m.lno;
	SCRCNO(ep) = m.cno;
}

static int
autoindent(ep, lno, lenp)
	EXF *ep;
	recno_t lno;
	size_t *lenp;
{
	size_t len, nlen;
	u_char *p, *t;
	
	/* Default is 0. */
	*lenp = 0;

	if ((p = t = file_gline(ep, lno, &len)) == NULL)
		return (0);
	for (nlen = 0; len; ++p) {
		if (!isspace(*p))
			break;
		/* If last character is a space, it counts. */
		if (--len == 0) {
			++p;
			break;
		}
	}

	/* No indentation. */
	if (p == t)
		return (0);

	/* Set count. */
	nlen = p - t;

	/* Make sure the buffer's big enough. */
	BINC(ep, ib.ilb, ib.ilblen, nlen + ib.len);

	/* Copy the indentation into the new buffer. */
	memmove(ib.ilb + nlen, ib.ilb, nlen);
	memmove(ib.ilb, t, nlen);
	ib.len += nlen;

	/* Return the additional length. */
	*lenp = nlen;
	return (0);
}
