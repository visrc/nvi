/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cut.c,v 5.11 1992/05/21 13:02:12 bostic Exp $ (Berkeley) $Date: 1992/05/21 13:02:12 $";
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
#include "mark.h"
#include "cut.h"
#include "options.h"
#include "pathnames.h"
#include "extern.h"

static int	cutline __P((recno_t, size_t, size_t, CBLINE **));
static void	freecbline __P((CBLINE *));

CB cuts[UCHAR_MAX + 1];		/* Set of cut buffers. */

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
	CBLINE *cbl, *cblp;
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
		freecbline(cb->head);
		cb->head = NULL;
		cb->len = 0;
		cb->flags = 0;
	}

	/* Make cut from fm and to tm, swapping pointers if necessary. */
	if (fm->lno > tm->lno || fm->lno == tm->lno && fm->cno > tm->cno) {
		m = *fm;
		*fm = *tm;
		*tm = m;
		append = 0;
	} else
		append = 1;
		
#if DEBUG && 1
	TRACE("cut: from {%lu, %d}, to {%lu, %d}\n",
	    fm->lno, fm->cno, tm->lno, tm->cno);
#endif

/* Append a new CBLINE structure into the CB chain. */
#define	CAPPEND {							\
	if ((cblp = cb->head) == NULL)					\
		cb->head = cbl;						\
	else {								\
		for (; cblp->next; cblp = cblp->next);			\
		cblp->next = cbl;					\
	}								\
	cb->len += cbl->len;						\
}

	if (lmode) {
		for (lno = fm->lno; lno <= tm->lno; ++lno) {
			if (cutline(lno, 0, 0, &cbl))
				goto mem;
			CAPPEND;
		}
		cb->flags |= CB_LMODE;
		return (0);
	}
		
	/* Get the first line. */
	len = fm->lno < tm->lno ? 0 : tm->cno - fm->cno;
	if (cutline(fm->lno, fm->cno, len, &cbl))
		goto mem;

	CAPPEND;

	for (lno = fm->lno; ++lno < tm->lno;) {
		if (cutline(lno, 0, 0, &cbl))
			goto mem;
		CAPPEND;
	}

	if (tm->lno > fm->lno && tm->cno > 0) {
		if (cutline(lno, 0, tm->cno, &cbl)) {
mem:			if (append)
				msg("Contents of buffer %s lost.",
				    charname(buffer));
			freecbline(cb->head);
			cb->head = NULL;
			return (1);
		}
		CAPPEND;
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
	CBLINE **newp;
{
	CBLINE *cp;
	size_t llen;
	char *lp, *p;

	EGETLINE(p, lno, llen);
	if ((cp = malloc(sizeof(CBLINE))) == NULL)
		goto mem;
	if (llen == 0) {
		cp->lp = NULL;
		cp->len = 0;
#if DEBUG && 1
		TRACE("{}\n");
#endif
	} else {
		if (len == 0)
			len = llen - fcno;
		if ((lp = malloc(len)) == NULL) {
			free(cp);
mem:			bell();
			msg("Error: %s", strerror(errno));
			return (1);
		}
		bcopy(p + fcno, lp, len);
		cp->lp = lp;
		cp->len = len;
#if DEBUG && 1
		TRACE("\t{%.*s}\n", MIN(len, 20), p + fcno);
#endif
	}
	cp->next = NULL;
	*newp = cp;
	return (0);
}

/* Increase the buffer space as necessary. */
#define	BFCHECK {							\
	if (blen < len + cblp->len) {					\
		blen += MAX(256, len + cblp->len);			\
		if ((bp = realloc(bp, blen)) == NULL) {			\
			msg("Error: %s", strerror(errno));		\
			blen = 0;					\
			return (1);					\
		}							\
	}								\
}

/*
 * put --
 *	Put cut buffer contents back into the text.
 */	
int
put(buffer, cp, rp, append)
	int buffer, append;
	MARK *cp, *rp;
{
	static char *bp;
	static size_t blen;
	CB *cb;
	CBLINE *cblp;
	recno_t lno;
	size_t clen, len;
	int intermediate;
	char *p, *t;

	CBNAME(buffer, cb);
	CBEMPTY(buffer, cb);

	/*
	 * If buffer was cut in line mode, append each new line into the
	 * file.  Otherwise, insert the first line into place, append
	 * each new line into the file, and insert the last line into
	 * place.
	 *
	 * XXX
	 * We have to do some fairly interesting stuff to make this work
	 * for inserting above the first line.  This might be better done
	 * in db(3) by allowing record 0.
	 */
	if (cb->flags & CB_LMODE) {
		if (append) {
			for (lno = cp->lno, cblp = cb->head; cblp;
			    ++lno, cblp = cblp->next)
				if (file_aline(curf, lno, cblp->lp, cblp->len))
					return (1);
			rp->lno = cp->lno + 1;
		} else if ((lno = cp->lno) != 1) {
			for (cblp = cb->head, --lno;
			    cblp; cblp = cblp->next, ++lno)
				if (file_aline(curf, lno, cblp->lp, cblp->len))
					return (1);
			rp->lno = cp->lno;
		} else {
			cblp = cb->head;
			if (file_iline(curf, (recno_t)1, cblp->lp, cblp->len))
				return (1);
			for (lno = 1; cblp = cblp->next; ++lno)
				if (file_aline(curf, lno, cblp->lp, cblp->len))
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
		cblp = cb->head;

		/* Get the first line. */
		lno = cp->lno;
		EGETLINE(p, lno, len);

		/* Check for space. */
		BFCHECK;

		/* Original line, left of the split. */
		t = bp;
		if (len > 0 && (clen = cp->cno + (append ? 1 : 0)) > 0) {
			bcopy(p, bp, clen);
			p += clen;
			t += clen;
		}

		/* First line from the CB. */
		bcopy(cblp->lp, t, cblp->len);
		t += cblp->len;

		/* Calculate length left in original line. */
		clen = len ? len - cp->cno - (append ? 1 : 0) : 0;

		/*
		 * If no more lines in the CB, append the rest of the original
		 * line and quit.
		 */
		if (cblp->next == NULL) {
			rp->lno = lno;
			rp->cno = t - bp;

			if (clen > 0) {
				bcopy(p, t, clen);
				t += clen;
			}
			if (file_sline(curf, lno, bp, t - bp))
				return (1);

		} else {
			/* Output the line replacing the original line. */
			if (file_sline(curf, lno, bp, t - bp))
				return (1);

			/* Output any intermediate lines in the CB alone. */
			for (;;) {
				cblp = cblp->next;
				if (cblp->next == NULL)
					break;
				if (file_aline(curf, lno, cblp->lp, cblp->len))
					return (1);
				++lno;
			}

			/* Last part of original line. */

			/* Check for space. */
			BFCHECK;

			t = bp;
			bcopy(cblp->lp, t, cblp->len);
			t += cblp->len;

			rp->lno = lno;
			rp->cno = t - bp;

			if (clen) {
				bcopy(p, t, clen);
				t += clen;
			}
			if (file_aline(curf, lno, bp, t - bp))
				return (1);
		}
	}

	/* Ping the screen. */
	scr_ref();

	/* Shift any marks in the range. */
	mark_insert(cp, rp);

	/* Reporting... */
	rptlabel = "put";
	rptlines = lno - cp->lno;

	return (0);
}

/*
 * freecbline --
 *	Free a chain of cbline structures.
 */
static void
freecbline(cp)
	CBLINE *cp;
{
	CBLINE *np;

	do {
		np = cp->next;
		free(cp);
	} while (cp = np);
}
