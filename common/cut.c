/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cut.c,v 5.17 1992/10/18 13:02:13 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:02:13 $";
#endif /* not lint */

#include <sys/param.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

static int	cutline __P((recno_t, size_t, size_t, TEXT **));

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
cut(buffer, fm, tm, lmode)
	int buffer, lmode;
	MARK *fm, *tm;
{
	CB *cb;
	TEXT *tp;
	MARK m;
	size_t len;
	recno_t lno;
	int append;

	CBNAME(buffer, cb);

	/*
	 * Upper-case buffer names map into lower-case buffers, but with
	 * append mode set so the buffer is appended to, not overwritten.
	 * Also note if the buffer has changed modes.
	 */
	if (append = isupper(buffer))
		if (!lmode && cb->flags & CB_LMODE)
			msg("Buffer %s changed to line mode.",
			    charname(buffer));

	/* Free old buffer. */
	if (cb->head != NULL && !append) {
		freetext(cb->head);
		cb->head = NULL;
		cb->len = 0;
		cb->flags = 0;
	}

#if DEBUG && 1
	TRACE("cut: from {%lu, %d}, to {%lu, %d}%s\n",
	    fm->lno, fm->cno, tm->lno, tm->cno, lmode ? " LINE MODE" : "");
#endif

	if (lmode) {
		for (lno = fm->lno; lno <= tm->lno; ++lno) {
			if (cutline(lno, 0, 0, &tp))
				goto mem;
			TEXTAPPEND(cb, tp);
		}
		cb->flags |= CB_LMODE;
		return (0);
	}
		
	/* Get the first line. */
	len = fm->lno < tm->lno ? 0 : tm->cno - fm->cno;
	if (cutline(fm->lno, fm->cno, len, &tp))
		goto mem;

	TEXTAPPEND(cb, tp);

	for (lno = fm->lno; ++lno < tm->lno;) {
		if (cutline(lno, 0, 0, &tp))
			goto mem;
		TEXTAPPEND(cb, tp);
	}

	if (tm->lno > fm->lno && tm->cno > 0) {
		if (cutline(lno, 0, tm->cno, &tp)) {
mem:			if (append)
				msg("Contents of buffer %s lost.",
				    charname(buffer));
			freetext(cb->head);
			cb->head = NULL;
			return (1);
		}
		TEXTAPPEND(cb, tp);
	}
	return (0);
}

/*
 * cutline --
 *	Cut a portion of a single line.  A length of zero means to cut
 * 	from the MARK to the end of the line.
 */
static int
cutline(lno, fcno, len, newp)
	recno_t lno;
	size_t fcno, len;
	TEXT **newp;
{
	TEXT *tp;
	size_t llen;
	u_char *lp, *p;

	if ((p = file_gline(curf, lno, &llen)) == NULL) {
		GETLINE_ERR(lno);
		return (1);
	}

	if ((tp = malloc(sizeof(TEXT))) == NULL)
		goto mem;
	if (llen == 0) {
		tp->lp = NULL;
		tp->len = 0;
#if DEBUG && 1
		TRACE("{}\n");
#endif
	} else {
		if (len == 0)
			len = llen - fcno;
		if ((lp = malloc(len)) == NULL) {
			free(tp);
mem:			bell();
			msg("Error: %s", strerror(errno));
			return (1);
		}
		bcopy(p + fcno, lp, len);
		tp->lp = lp;
		tp->len = len;
#if DEBUG && 1
		TRACE("\t{%.*s}\n", MIN(len, 20), p + fcno);
#endif
	}
	tp->next = NULL;
	*newp = tp;
	return (0);
}

/* Increase the buffer space as necessary. */
#define	BFCHECK(tp) {							\
	if (blen < len + tp->len) {					\
		blen += MAX(blen + 256, len + tp->len);			\
		if ((bp = realloc(bp, blen)) == NULL) {			\
			msg("Put error: %s", strerror(errno));		\
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
put(buffer, cp, rp, append)
	int buffer, append;
	MARK *cp, *rp;
{
	static u_char *bp;
	static size_t blen;
	CB *cb;
	TEXT *tp;
	recno_t lno;
	size_t clen, len;
	int intermediate, lmode;
	u_char *p, *t;

	CBNAME(buffer, cb);
	CBEMPTY(buffer, cb);
	tp = cb->head;
	lmode = cb->flags & CB_LMODE;

	/*
	 * If buffer was created in line mode, append each new line into the
	 * file.  Otherwise, insert the first line into place, append each
	 * new line into the file, and insert the last line into place.
	 *
	 * XXX
	 * We have to do some fairly interesting stuff to make this work
	 * for inserting above the first line.  This might be better done
	 * in db(3) by allowing record 0.
	 */
	if (lmode) {
		if (append) {
			for (lno = cp->lno; tp; ++lno, tp = tp->next)
				if (file_aline(curf, lno, tp->lp, tp->len))
					return (1);
			rp->lno = cp->lno + 1;
		} else if ((lno = cp->lno) != 1) {
			for (--lno; tp; tp = tp->next, ++lno)
				if (file_aline(curf, lno, tp->lp, tp->len))
					return (1);
			rp->lno = cp->lno;
		} else {
			if (file_iline(curf, (recno_t)1, tp->lp, tp->len))
				return (1);
			for (lno = 1; tp = tp->next; ++lno)
				if (file_aline(curf, lno, tp->lp, tp->len))
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
		if ((p = file_gline(curf, lno, &len)) == NULL) {
			GETLINE_ERR(lno);
			return (1);
		}

		/* Check for space. */
		BFCHECK(tp);

		/* Original line, left of the split. */
		t = bp;
		if (len > 0 && (clen = cp->cno + (append ? 1 : 0)) > 0) {
			bcopy(p, bp, clen);
			p += clen;
			t += clen;
		}

		/* First line from the CB. */
		bcopy(tp->lp, t, tp->len);
		t += tp->len;

		/* Calculate length left in original line. */
		clen = len ? len - cp->cno - (append ? 1 : 0) : 0;

		/*
		 * If no more lines in the CB, append the rest of the original
		 * line and quit.
		 */
		if (tp->next == NULL) {
			if (clen > 0) {
				bcopy(p, t, clen);
				t += clen;
			}
			if (file_sline(curf, lno, bp, t - bp))
				return (1);

			rp->lno = lno;
			rp->cno = t - bp;
		} else {
			/* Output the line replacing the original line. */
			if (file_sline(curf, lno, bp, t - bp))
				return (1);

			/* Output any intermediate lines in the CB alone. */
			for (;;) {
				tp = tp->next;
				if (tp->next == NULL)
					break;
				if (file_aline(curf, lno, tp->lp, tp->len))
					return (1);
				++lno;
			}

			/* Last part of original line; check for space. */
			BFCHECK(tp);

			t = bp;
			if (tp->len) {
				bcopy(tp->lp, t, tp->len);
				t += tp->len;
			}

			/* This is the end of the added text; set cursor. */
			rp->lno = lno + 1;
			rp->cno = t - bp - 1;

			if (clen) {
				bcopy(p, t, clen);
				t += clen;
			}
			if (file_aline(curf, lno, bp, t - bp))
				return (1);
		}
	}

	/* Shift any marks in the range. */
	mark_insert(cp, rp);

	/* Reporting... */
	rptlabel = "put";
	rptlines = lno - cp->lno;

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
