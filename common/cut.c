/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cut.c,v 8.13 1993/11/18 10:08:30 bostic Exp $ (Berkeley) $Date: 1993/11/18 10:08:30 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static int	cb_line __P((SCR *, EXF *, recno_t, size_t, size_t, TEXT **));
static int	cb_rotate __P((SCR *));

/* 
 * cut --
 *	Put a range of lines/columns into a buffer.
 */
int
cut(sp, ep, name, fm, tm, lmode)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T name;
	int lmode;
	MARK *fm, *tm;
{
	CB *cbp;
	TEXT *tp;
	recno_t lno;
	size_t len;
	int append;

#if defined(DEBUG) && 0
	TRACE(sp, "cut: from {%lu, %d}, to {%lu, %d}%s\n",
	    fm->lno, fm->cno, tm->lno, tm->cno, lmode ? " LINE MODE" : "");
#endif
	/*
	 * !!!
	 * The numeric buffers in historic vi offer us yet another opportunity
	 * for contemplation.  First, the default buffer was the same as buffer
	 * '1'.  Second, cutting into any numeric buffer caused buffers '1' to
	 * '8' to be rotated up one, and '9' to drop off the end.  Finally,
	 * text cut into a numeric buffer other than '1' was always appended
	 * to the buffer (after the rotation), it was not a replacement.
	 */
	append = 0;
	if (isdigit(name)) {
		(void)cb_rotate(sp);
		if (name != '1')
			append = 1;
	}

	/*
	 * Upper-case buffer names map into lower-case buffers, but with
	 * append mode set so the buffer is appended to, not overwritten.
	 */
	if (isupper(name))
		append = 1;
	CBNAME(sp, cbp, name);

	/*
	 * If this is a new buffer, create it, and add it into the list.
	 * Otherwise, if it's not an append, free its current contents.
	 */
	if (cbp == NULL) {
		if ((cbp = malloc(sizeof(CB))) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
		memset(cbp, 0, sizeof(CB));
		cbp->name = name;
		LIST_INSERT_HEAD(&sp->gp->cutq, cbp, q);
		CIRCLEQ_INIT(&cbp->textq);
	} else if (!append) {
		text_lfree(&cbp->textq);
		cbp->len = 0;
		cbp->flags = 0;
	}

	if (lmode) {
		for (lno = fm->lno; lno <= tm->lno; ++lno) {
			if (cb_line(sp, ep, lno, 0, 0, &tp))
				goto mem;
			CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
			cbp->len += tp->len;
		}
		cbp->flags |= CB_LMODE;
		return (0);
	}
		
	/* Get the first line. */
	len = fm->lno < tm->lno ? 0 : tm->cno - fm->cno;
	if (cb_line(sp, ep, fm->lno, fm->cno, len, &tp))
		goto mem;

	CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
	cbp->len += tp->len;

	for (lno = fm->lno; ++lno < tm->lno;) {
		if (cb_line(sp, ep, lno, 0, 0, &tp))
			goto mem;
		CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
		cbp->len += tp->len;
	}

	if (tm->lno > fm->lno && tm->cno > 0) {
		if (cb_line(sp, ep, lno, 0, tm->cno, &tp)) {
mem:			if (append)
				msgq(sp, M_ERR,
				    "Contents of %s buffer lost.",
				    charname(sp, name));
			text_lfree(&cbp->textq);
			cbp->len = 0;
			cbp->flags = 0;
			return (1);
		}
		CIRCLEQ_INSERT_TAIL(&cbp->textq, tp, q);
		cbp->len += tp->len;
	}
	return (0);
}

/*
 * cb_rotate --
 *	Rotate the numbered buffers up one.
 */
static int
cb_rotate(sp)
	SCR *sp;
{
	CB *cbp, *del_cbp;

	del_cbp = NULL;
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next)
		switch(cbp->name) {
		case '1':
			cbp->name = '2';
			break;
		case '2':
			cbp->name = '3';
			break;
		case '3':
			cbp->name = '4';
			break;
		case '4':
			cbp->name = '5';
			break;
		case '5':
			cbp->name = '6';
			break;
		case '6':
			cbp->name = '7';
			break;
		case '7':
			cbp->name = '8';
			break;
		case '8':
			cbp->name = '9';
			break;
		case '9':
			del_cbp = cbp;
			break;
		}
	if (del_cbp != NULL) {
		LIST_REMOVE(del_cbp, q);
		text_lfree(&del_cbp->textq);
		FREE(del_cbp, sizeof(CB));
	}
	return (0);
}

/*
 * cb_line --
 *	Cut a portion of a single line.
 */
static int
cb_line(sp, ep, lno, fcno, clen, newp)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t fcno, clen;
	TEXT **newp;
{
	TEXT *tp;
	size_t len;
	char *p;

	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}

	if ((*newp = tp = text_init(sp, NULL, 0, len)) == NULL)
		return (1);

	/*
	 * A length of zero means to cut from the MARK to the end
	 * of the line.
	 */
	if (len != 0) {
		if (clen == 0)
			clen = len - fcno;
		memmove(tp->lb, p + fcno, clen);
		tp->len = clen;
	}
	return (0);
}

/*
 * text_init --
 *	Allocate a new TEXT structure.
 */
TEXT *
text_init(sp, p, len, total_len)
	SCR *sp;
	char *p;
	size_t len, total_len;
{
	TEXT *tp;

	if ((tp = malloc(sizeof(TEXT))) == NULL)
		goto mem;
	if ((tp->lb = malloc(tp->lb_len = total_len)) == NULL) {
		free(tp);
mem:		msgq(sp, M_SYSERR, NULL);
		return (NULL);
	}
#ifdef DEBUG
	if (total_len)
		memset(tp->lb, 0, total_len - 1);
#endif

	if (p != NULL && len != 0)
		memmove(tp->lb, p, len);
	tp->len = len;
	tp->ai = tp->insert = tp->offset = tp->owrite = 0;
	tp->wd = NULL;
	tp->wd_len = 0;

	return (tp);
}

/*
 * text_lfree --
 *	Free a chain of text structures.
 */
void
text_lfree(headp)
	TEXTH *headp;
{
	TEXT *tp;

	while ((tp = headp->cqh_first) != (void *)headp) {
		CIRCLEQ_REMOVE(headp, tp, q);
		text_free(tp);
	}
}

/*
 * text_free --
 *	Free a text structure.
 */
void
text_free(tp)
	TEXT *tp;
{
	if (tp->lb != NULL)
		FREE(tp->lb, tp->lb_len);
	if (tp->wd != NULL)
		FREE(tp->wd, tp->wd_len);
	FREE(tp, sizeof(TEXT));
}

/*
 * put --
 *	Put text buffer contents into the file.
 *
 * !!!
 * Historically, pasting into a file with no lines in vi would preserve
 * the single blank line.  This is almost certainly a result of the fact
 * that historic vi couldn't deal with a file that had no lines in it.
 * This implementation treats that as a bug, and does not retain the
 * blank line.
 */	
int
put(sp, ep, name, cp, rp, append)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T name;
	MARK *cp, *rp;
	int append;
{
	CB *cbp;
	TEXT *ltp, *tp;
	recno_t lno;
	size_t blen, clen, len;
	int lmode;
	char *bp, *p, *t;

	CBEMPTY(sp, cbp, name);

	tp = cbp->textq.cqh_first;
	lmode = F_ISSET(cbp, CB_LMODE);

	/*
	 * It's possible to do a put into an empty file, meaning that the
	 * cut buffer simply becomes the file.  It's a special case so
	 * that we can ignore it in general.
	 *
	 * Historical practice is that the cursor ends up on the first
	 * non-blank character of the first line inserted.
	 */
	if (cp->lno == 1) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0) {
			for (; tp != (void *)&cbp->textq;
			     ++lno, tp = tp->q.cqe_next)
				if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
					return (1);
			rp->lno = 1;
			rp->cno = 0;
			(void)nonblank(sp, ep, rp->lno, &rp->cno);
			goto ret;
		}
	}
			
	/*
	 * If buffer was created in line mode, append each new line into the
	 * file.
	 */
	if (lmode) {
		lno = append ? cp->lno : cp->lno - 1;
		rp->lno = lno + 1;
		for (; tp != (void *)&cbp->textq; ++lno, tp = tp->q.cqe_next)
			if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
				return (1);
		rp->cno = 0;
		(void)nonblank(sp, ep, rp->lno, &rp->cno);
		goto ret;
	}

	/*
	 * If buffer was cut in character mode, replace the current line with
	 * one built from the portion of the first line to the left of the
	 * split plus the first line in the CB.  Append each intermediate line
	 * in the CB.  Append a line built from the portion of the first line
	 * to the right of the split plus the last line in the CB.
	 *
	 * Get the first line.
	 */
	lno = cp->lno;
	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}

	GET_SPACE(sp, bp, blen, tp->len + len + 1);
	t = bp;

	/* Original line, left of the split. */
	if (len > 0 && (clen = cp->cno + (append ? 1 : 0)) > 0) {
		memmove(bp, p, clen);
		p += clen;
		t += clen;
	}

	/* First line from the CB. */
	memmove(t, tp->lb, tp->len);
	t += tp->len;

	/* Calculate length left in original line. */
	clen = len ? len - cp->cno - (append ? 1 : 0) : 0;

	/*
	 * If no more lines in the CB, append the rest of the original
	 * line and quit.  Otherwise, build the last line before doing
	 * the intermediate lines, because the line changes will lose
	 * the cached line.
	 */
	if (tp->q.cqe_next == (void *)&cbp->textq) {
		/*
		 * Historical practice is that if a non-line mode put
		 * is inside a single line, the cursor ends up on the
		 * last character inserted.
		 */
		rp->lno = lno;
		rp->cno = (t - bp) - 1;

		if (clen > 0) {
			memmove(t, p, clen);
			t += clen;
		}
		if (file_sline(sp, ep, lno, bp, t - bp))
			goto mem;
	} else {
		/*
		 * Have to build both the first and last lines of the
		 * put before doing any sets or we'll lose the cached
		 * line.  Build both the first and last lines in the
		 * same buffer, so we don't have to have another buffer
		 * floating around.
		 *
		 * Last part of original line; check for space, reset
		 * the pointer into the buffer.
		 */
		ltp = cbp->textq.cqh_last;
		len = t - bp;
		ADD_SPACE(sp, bp, blen, ltp->len + clen);
		t = bp + len;

		/* Add in last part of the CB. */
		memmove(t, ltp->lb, ltp->len);
		if (clen)
			memmove(t + ltp->len, p, clen);
		clen += ltp->len;

		/*
		 * Now: bp points to the first character of the first
		 * line, t points to the last character of the last
		 * line, t - bp is the length of the first line, and
		 * clen is the length of the last.  Just figured you'd
		 * want to know.
		 *
		 * Output the line replacing the original line.
		 */
		if (file_sline(sp, ep, lno, bp, t - bp))
			goto mem;

		/*
		 * Historical practice is that if a non-line mode put
		 * covers multiple lines, the cursor ends up on the
		 * first character inserted.  (Of course.)
		 */
		rp->lno = lno;
		rp->cno = (t - bp) - 1;

		/* Output any intermediate lines in the CB. */
		for (tp = tp->q.cqe_next;
		    tp->q.cqe_next != (void *)&cbp->textq;
		    ++lno, tp = tp->q.cqe_next)
			if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
				goto mem;

		if (file_aline(sp, ep, 1, lno, t, clen)) {
mem:			FREE_SPACE(sp, bp, blen);
			return (1);
		}
	}
	FREE_SPACE(sp, bp, blen);

	/* Reporting... */
ret:	sp->rptlines[L_PUT] += lno - cp->lno;

	return (0);
}
