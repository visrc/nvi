/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_txt.c,v 5.1 1993/04/06 11:17:04 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:17:04 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "text.h"

static int	autoindent __P((SCR *, EXF *, recno_t, size_t *));
static void	ib_err __P((SCR *, EXF *));

#define	SIB	sp->ib

/* Allocate a new TEXT structure. */
#define	NEWTP(sp) {							\
	if ((tp = malloc(sizeof(TEXT))) == NULL ||			\
	    (tp->lp = malloc(SIB.len)) == NULL) {			\
		if (tp != NULL)						\
			free(tp);					\
		msgq(sp, M_ERROR, "Error: %s.", strerror(errno));	\
		eval = 1;						\
		goto done;						\
	}								\
	memmove(tp->lp, SIB.ilb, SIB.len);				\
	tp->len = SIB.len;						\
	tp->next = NULL;						\
	TEXTAPPEND(&SIB, tp);						\
}

#define	SCREEN_UPDATE(sp) {						\
	if (sp->refresh(sp, ep)) {					\
		eval = 1;						\
		goto done;						\
	}								\
}

/*
 * newtext --
 *	Read in text from the user.
 *
 * XXX
 * Make quoted an enum.
 */
int
v_ntext(sp, ep, vp, tm, p, len, rp, ai_line, flags)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *tm;		/* To MARK. */
	char *p;		/* Input line, then input buffer pointer. */
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
	char *repp;		/* Replay buffer. */

	/* Set input flag. */
	F_SET(sp, S_INPUT);

	/* Set the initial cursor position. */
	SIB.start.lno = SIB.stop.lno = sp->lno;
	SIB.start.cno = SIB.stop.cno = sp->cno;

	/* Make sure the buffer is big enough even if the line is empty. */
	if (binc(sp, &SIB.ilb, &SIB.ilblen, len + 5))
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
		memmove(SIB.ilb, p, len);
		SIB.len = len;
		if (flags & N_OVERWRITE) {
			overwrite = tm->cno - sp->cno;
			insert = len - tm->cno;
		} else {
			overwrite = 0;
			insert = len - sp->cno;
		}
		if (flags & N_EMARK)
			SIB.ilb[tm->cno - 1] = END_CH;
	} else {
		SIB.len = 0;
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
		if (autoindent(sp, ep, ai_line, &col))
			return (1);
		in_ai = 1;
		startcol = 0;
		sp->cno = col ? col - 1 : 0;
	} else {
		in_ai = 0;
		col = startcol = sp->cno;
	}

	/* Point to the first empty slot in which to insert a character. */
	p = SIB.ilb + col;

	/*
	 * If appending after the end-of-line, add a space into the buffer
	 * and move the cursor right.  This space is inserted, i.e. pushed
	 * along, and then deleted when the line is resolved.  Assumes that
	 * the cursor is already positioned at the end of the line.
	 */
	if (flags & N_APPENDEOL) {
		*p = '+';
		sp->cno = SIB.len;
		++SIB.len;
		++insert;
	}

	/* Reset the line and update the screen. */
	if (sp->change(sp, ep, SIB.start.lno, LINE_RESET))
		return (1);
	SCREEN_UPDATE(sp);
		
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
	repp = SIB.rep;
	replay = vp->flags & VC_ISDOT;

	for (carat_st = C_NOTSET, eval = quoted = 0;;) {
next_ch:	if (replay)
			ch = *repp++;
		else {
			/*
			 * Check if the character fits into the input and
			 * replay buffers; allocate space as necesssary.
			 */
			if (col + insert >= SIB.ilblen) {
				if (binc(sp, &SIB.ilb, &SIB.ilblen, 0)) {
					eval = 1;
					goto done;
				}
				p = SIB.ilb + col;
			}

			if (rcol >= SIB.replen) {
				if (binc(sp, &SIB.rep, &SIB.replen, 0)) {
					eval = 1;
					goto done;
				}
				repp = SIB.rep + rcol;
			}
			/* Store the character into the replay buffer. */
			*repp++ = ch = getkey(sp, GB_BEAUTIFY | GB_MAPINPUT);
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
			--sp->cno;
			goto ins_qch;
		}

		switch(sp->special[ch]) {
		case K_ESCAPE:				/* Escape. */
			/*
			 * If the user hasn't inserted any characters,
			 * delete the autoindent characters.
			 *
			 * If we haven't been backspacing over characters
			 * already, delete any appended space character.
			 */
			if (in_ai) {
				p = SIB.ilb;
				SIB.len = 0;
				F_SET(sp, S_CHARDELETED);
			} else if (flags & N_APPENDEOL) {
				--p;
				--SIB.len;
				--insert;
				F_SET(sp, S_CHARDELETED);
			}

			/*
			 * The cursor normally follows the last inserted
			 * character.  Reset the return cursor position
			 * to rest on the last inserted character.
			 */
			if (sp->cno)
				--sp->cno;
			SIB.stop.cno = sp->cno;

			/* If no input, just return. */
			if (SIB.start.lno == SIB.stop.lno &&
			    SIB.start.cno == SIB.stop.cno)
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
			sp->change(sp, ep, SIB.stop.lno, LINE_RESET);
			SCREEN_UPDATE(sp);

			/* Append the line into the text structure. */
			NEWTP(sp);

			/* Resolve the input lines into the file. */
			eval = file_ibresolv(sp, ep, SIB.start.lno);
			goto done;
		case K_CR:
		case K_NL:				/* New line. */
			/*
			 * If the user hasn't inserted any characters,
			 * delete the autoindent characters.
			 */
			if (in_ai) {
				col = 0;
				F_SET(sp, S_CHARDELETED);
			}

			/* Ignore the rest of the line. */
			SIB.len = col;

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
			NEWTP(sp);

			/*
			 * Update the screen.  Update the current line number
			 * so the line is retrieved from the TEXT structure.
			 */
			++SIB.stop.lno;
			if (sp->change(sp, ep, SIB.stop.lno - 1, LINE_RESET)) {
				eval = 1;
				goto done;
			}

			/*
			 * Copy the remaining characters to the start of the
			 * input buffer, reset the buffer.
			 */
			if (insert) {
				memmove(SIB.ilb, p, insert);
				SIB.len = insert;
			} else
				SIB.len = 0;

			/* Reset the autoindent if necessary. */
			if (carat_st == C_NOINC)
				carat_st = C_NOTSET;
			else if (!in_ai)
				ai_line = SIB.stop.lno - 1;

			/* Reset the input buffer, adding any autoindent. */
			startcol = 0;
			if (ISSET(O_AUTOINDENT)) {
				if (autoindent(sp, ep, ai_line, &col)) {
					eval = 1;
					goto done;
				}
				in_ai = 1;
			} else
				col = 0;
			SIB.len += col;
			p = SIB.ilb + col;
			
			/* Reset the cursor. */
			sp->lno = SIB.stop.lno;
			sp->cno = col;
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
				sp->bell(sp);
				break;
			}
			/*
			 * Drop back one character.  If in autoindent, delete
			 * the character, otherwise, increment the overwrite
			 * count.
			 */
			--p;
			--col;
			--sp->cno;
			if (in_ai) {
				p[0] = p[1];
				--SIB.len;
				F_SET(sp, S_CHARDELETED);
			} else
				++overwrite;
			break;
		case K_VWERASE:			/* Skip back one word. */
			/* Check for nothing to erase. */
werase:			if (col == startcol) {
				sp->bell(sp);
				break;
			}
			/* Skip over space characters. */
			while (col > startcol && isspace(p[-1])) {
				--p;
				--col;
				--sp->cno;

				/* If in autoindent, just lose the character. */
				if (in_ai) {
					p[0] = p[1];
					--SIB.len;
				} else
					++overwrite;
			}
			if (in_ai)
				F_SET(sp, S_CHARDELETED);
			if (col == startcol)
				break;
			for (tmp = inword(p[-1]); col > startcol;) {
				++overwrite;
				--p;
				--col;
				--sp->cno;
				if (tmp != inword(p[-1]))
					break;
			}
			break;
		case K_VKILL:			/* Restart this line. */
			col = sp->cno = startcol;
			p = SIB.ilb + col;
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
				F_SET(sp, S_CHARDELETED);
			} else if (insert)
				memmove(p + 1, p, insert);
ins_qch:		*p++ = ch;
			++col;
			++sp->cno;
			if (carat_st == C_ZEROSET || carat_st == C_CARATSET) {
				ch = carat_ch;
				carat_st = C_NOTSET;
				goto carat_lable;
			}
			in_ai = 0;
			break;
		}
		SIB.len = col + insert + overwrite;
		sp->change(sp, ep, SIB.stop.lno, !quoted &&
		    (sp->special[ch] == K_NL || sp->special[ch] == K_CR) ?
		    LINE_INSERT : LINE_RESET);
		SCREEN_UPDATE(sp);
		if (quoted)
			--quoted;
	}

	/*
	 * Adjust the cursor.  If an error occurred, ib_err() makes sure
	 * the cursor is rational.
	 */
done:	if (eval == 1)
		ib_err(sp, ep);
	else {
		rp->lno = SIB.stop.lno;
		rp->cno = SIB.stop.cno;
	}

	/* Free up text buffer, turn off look-aside check. */
	if (SIB.head != NULL) {
		text_free(SIB.head);
		SIB.head = NULL;
	}
	SIB.stop.lno = OOBLNO;

	/* Clear input flag. */
	F_CLR(sp, S_INPUT);

	return (eval);
}

/*
 * ib_err --
 *	Handle an error during input processing.
 */
static void
ib_err(sp, ep)
	SCR *sp;
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
	for (m = SIB.start;
	    file_gline(sp, ep, m.lno, &len) == NULL && --m.lno > 0;);
	if (m.lno == 0)
		m.cno = 0;
	else if (m.cno >= len)
		m.cno = len ? len - 1 : 0;

	sp->lno = m.lno;
	sp->cno = m.cno;
}

static int
autoindent(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t *lenp;
{
	size_t len, nlen;
	char *p, *t;
	
	/* Default is 0. */
	*lenp = 0;

	if ((p = t = file_gline(sp, ep, lno, &len)) == NULL)
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
	BINC(sp, SIB.ilb, SIB.ilblen, nlen + SIB.len);

	/* Copy the indentation into the new buffer. */
	memmove(SIB.ilb + nlen, SIB.ilb, nlen);
	memmove(SIB.ilb, t, nlen);
	SIB.len += nlen;

	/* Return the additional length. */
	*lenp = nlen;
	return (0);
}
