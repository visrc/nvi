/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cut.c,v 5.29 1993/03/26 13:37:36 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:37:36 $";
#endif /* not lint */

#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static int	cutline __P((SCR *, EXF *, recno_t, size_t, size_t, TEXT **));

CB cuts[UCHAR_MAX + 2];		/* Set of cut buffers. */

/* 
 * cut --
 *	Put a range of lines/columns into a malloced buffer for use
 *	later.
 * XXX
 * If the range is over some set amount (LINES lines?) should fall
 * back and use ex_write to dump the cut into a temporary file.
 */
int
cut(sp, ep, buffer, fm, tm, lmode)
	SCR *sp;
	EXF *ep;
	int buffer, lmode;
	MARK *fm, *tm;
{
	CB *cb;
	TEXT *tp;
	size_t len;
	recno_t lno;
	int append;

	CBNAME(sp, buffer, cb);

	/*
	 * Upper-case buffer names map into lower-case buffers, but with
	 * append mode set so the buffer is appended to, not overwritten.
	 * Also note if the buffer has changed modes.
	 */
	if (append = isupper(buffer))
		if (!lmode && cb->flags & CB_LMODE)
			msgq(sp, M_DISPLAY, "Buffer %s changed to line mode",
			    charname(sp, buffer));

	/* Free old buffer. */
	if (cb->head != NULL && !append) {
		freetext(cb->head);
		cb->head = NULL;
		cb->len = 0;
		cb->flags = 0;
	}

#if DEBUG && 0
	TRACE(sp->gp, "cut: from {%lu, %d}, to {%lu, %d}%s\n",
	    fm->lno, fm->cno, tm->lno, tm->cno, lmode ? " LINE MODE" : "");
#endif
	if (lmode) {
		for (lno = fm->lno; lno <= tm->lno; ++lno) {
			if (cutline(sp, ep, lno, 0, 0, &tp))
				goto mem;
			TEXTAPPEND(cb, tp);
			cb->len += tp->len;
		}
		cb->flags |= CB_LMODE;
		return (0);
	}
		
	/* Get the first line. */
	len = fm->lno < tm->lno ? 0 : tm->cno - fm->cno;
	if (cutline(sp, ep, fm->lno, fm->cno, len, &tp))
		goto mem;

	TEXTAPPEND(cb, tp);
	cb->len += tp->len;

	for (lno = fm->lno; ++lno < tm->lno;) {
		if (cutline(sp, ep, lno, 0, 0, &tp))
			goto mem;
		TEXTAPPEND(cb, tp);
		cb->len += tp->len;
	}

	if (tm->lno > fm->lno && tm->cno > 0) {
		if (cutline(sp, ep, lno, 0, tm->cno, &tp)) {
mem:			if (append)
				msgq(sp,
				    M_DISPLAY, "Contents of %s buffer lost.",
				    charname(sp, buffer));
			freetext(cb->head);
			cb->head = NULL;
			return (1);
		}
		TEXTAPPEND(cb, tp);
		cb->len += tp->len;
	}
	return (0);
}

/*
 * cutline --
 *	Cut a portion of a single line.  A length of zero means to cut
 * 	from the MARK to the end of the line.
 */
static int
cutline(sp, ep, lno, fcno, len, newp)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t fcno, len;
	TEXT **newp;
{
	TEXT *tp;
	size_t llen;
	u_char *lp, *p;

	if ((p = file_gline(sp, ep, lno, &llen)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}

	if ((tp = malloc(sizeof(TEXT))) == NULL)
		goto mem;
	if (llen == 0) {
		tp->lp = NULL;
		tp->len = 0;
#if DEBUG && 0
		TRACE(ep, "{}\n");
#endif
	} else {
		if (len == 0)
			len = llen - fcno;
		if ((lp = malloc(len)) == NULL) {
			free(tp);
mem:			msgq(sp, M_ERROR, "Error: %s", strerror(errno));
			return (1);
		}
		memmove(lp, p + fcno, len);
		tp->lp = lp;
		tp->len = len;
#if DEBUG && 0
		TRACE(ep, "\t{%.*s}\n", MIN(len, 20), p + fcno);
#endif
	}
	tp->next = NULL;
	*newp = tp;
	return (0);
}

/* Increase the buffer space as necessary. */
#define	BFCHECK(ep, tp) {						\
	if (blen < len + tp->len) {					\
		blen += MAX(blen + 256, len + tp->len);			\
		if ((bp = realloc(bp, blen)) == NULL) {			\
			msgq(sp, M_ERROR,				\
			    "Put error: %s", strerror(errno));		\
			blen = 0;					\
			return (1);					\
		}							\
	}								\
}

/*
 * put --
 *	Put text buffer contents into the file.
 */	
int
put(sp, ep, buffer, cp, rp, append)
	SCR *sp;
	EXF *ep;
	int buffer, append;
	MARK *cp, *rp;
{
	static u_char *bp;
	static size_t blen;
	CB *cb;
	TEXT *tp;
	recno_t lno;
	size_t clen, len;
	int lmode;
	u_char *p, *t;

	CBNAME(sp, buffer, cb);
	CBEMPTY(sp, buffer, cb);

	tp = cb->head;
	lmode = cb->flags & CB_LMODE;

	/*
	 * If buffer was created in line mode, append each new line into the
	 * file.  Otherwise, insert the first line into place, append each
	 * new line into the file, and insert the last line into place.
	 */
	if (lmode) {
		if (append) {
			for (lno = cp->lno; tp; ++lno, tp = tp->next)
				if (file_aline(sp, ep, lno, tp->lp, tp->len))
					return (1);
			rp->lno = cp->lno + 1;
		} else if ((lno = cp->lno) != 1) {
			for (--lno; tp; tp = tp->next, ++lno)
				if (file_aline(sp, ep, lno, tp->lp, tp->len))
					return (1);
			rp->lno = cp->lno;
		} else {
			if (file_iline(sp, ep, (recno_t)1, tp->lp, tp->len))
				return (1);
			for (lno = 1; tp = tp->next; ++lno)
				if (file_aline(sp, ep, lno, tp->lp, tp->len))
					return (1);
			rp->lno = 1;
		}
		rp->cno = 0;
	}
	/*
	 * If buffer was cut in character mode, replace the current line with
	 * one built from the portion of the first line to the left of the
	 * split plus the first line in the CB.  Append each intermediate line
	 * in the CB.  Append a line built from the portion of the first line
	 * to the right of the split plus the last line in the CB.
	 */
	else {
		/* Get the first line. */
		lno = cp->lno;
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			GETLINE_ERR(sp, lno);
			return (1);
		}

		/* Check for space. */
		BFCHECK(ep, tp);

		/* Original line, left of the split. */
		t = bp;
		if (len > 0 && (clen = cp->cno + (append ? 1 : 0)) > 0) {
			memmove(bp, p, clen);
			p += clen;
			t += clen;
		}

		/* First line from the CB. */
		memmove(t, tp->lp, tp->len);
		t += tp->len;

		/* Calculate length left in original line. */
		clen = len ? len - cp->cno - (append ? 1 : 0) : 0;

		/*
		 * If no more lines in the CB, append the rest of the original
		 * line and quit.
		 */
		if (tp->next == NULL) {
			if (clen > 0) {
				memmove(t, p, clen);
				t += clen;
			}
			if (file_sline(sp, ep, lno, bp, t - bp))
				return (1);

			rp->lno = lno;
			rp->cno = t - bp;
		} else {
			/* Output the line replacing the original line. */
			if (file_sline(sp, ep, lno, bp, t - bp))
				return (1);

			/* Output any intermediate lines in the CB alone. */
			for (;;) {
				tp = tp->next;
				if (tp->next == NULL)
					break;
				if (file_aline(sp, ep, lno, tp->lp, tp->len))
					return (1);
				++lno;
			}

			/* Last part of original line; check for space. */
			BFCHECK(ep, tp);

			t = bp;
			if (tp->len) {
				memmove(t, tp->lp, tp->len);
				t += tp->len;
			}

			/*
			 * This is the end of the added text; set cursor.
			 * XXX
			 * Historic vi put the cursor at the beginning of
			 * the put.
			 */
			rp->lno = lno + 1;
			rp->cno = t - bp - 1;

			if (clen) {
				memmove(t, p, clen);
				t += clen;
			}
			if (file_aline(sp, ep, lno, bp, t - bp))
				return (1);
		}
	}

	/* Shift any marks in the range. */
	mark_insert(sp, ep, cp, rp);

	/* Reporting... */
	sp->rptlabel = "put";
	sp->rptlines = lno - cp->lno;

	return (0);
}

/*
 * freetext --
 *	Free a chain of text structures.
 */
void
freetext(cp)
	TEXT *cp;
{
	TEXT *np;

	do {
		np = cp->next;
		free(cp);
	} while (cp = np);
}
