/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_word.c,v 5.3 1992/05/21 12:52:28 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:52:28 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

/*
 * There are two types of "words".  Bigwords are easy -- groups of anything
 * delimited by whitespace.  Normal words are trickier.  They are either a
 * group of characters, numbers and underscores, or a group of anything but,
 * delimited by whitespace.  When for a word, if you're in whitespace, it's
 * easy, just remove the whitespace and go to the beginning or end of the
 * word.  Otherwise, figure out if the next character is in a different group.
 * If it is, go to the beginning or end of that group, otherwise, go to the
 * beginning or end of the current group.  The historic version of vi didn't
 * get this right.  To get it right you have to resolve the cursor after each
 * search so that the look-ahead to figure out what type of "word" the cursor
 * is in will be correct.
 *
 * Empty lines count as a single word, and the beginning and end of the file
 * counts as an infinite number of words.
 */

#define	FW(test)	for (; len && (test); --len, ++p)
#define	BW(test)	for (; len && (test); --len, --p)

static int bword __P((VICMDARG *, MARK *, MARK *, int));
static int eword __P((VICMDARG *, MARK *, MARK *, int));
static int fword __P((VICMDARG *, MARK *, MARK *, int));

/*
 * v_wordw -- [count]w
 *	Move forward a word at a time.
 */
int
v_wordw(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (fword(vp, cp, rp, 0));
}

/*
 * v_wordW -- [count]W
 *	Move forward a bigword at a time.
 */
int
v_wordW(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (fword(vp, cp, rp, 1));
}

/*
 * fword --
 *	Move forward by words.
 */
static int
fword(vp, cp, rp, spaceonly)
	VICMDARG *vp;
	MARK *cp, *rp;
	int spaceonly;
{
	register char *p;
	size_t len;
	u_long cno, cnt, lno;
	int empty;
	char *startp;

	lno = cp->lno;
	cno = cp->cno;

	EGETLINE(p, lno, len);

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	/*
	 * Reset the length; the first character is the current cursor
	 * position.  If no more characters in this line, may already
	 * be at EOF.
	 */
	len -= cno;
	empty = len == 1;
	for (startp = p += cno; cnt--; empty = 0) {
		if (len != 0)
			if (spaceonly) {
				FW(!isspace(*p));
				FW(isspace(*p));
			} else {
				if (inword(*p))
					FW(inword(*p));
				else
					FW(!isspace(*p) && !inword(*p));
				FW(isspace(*p));
			}
		if (len == 0) {
			GETLINE(p, ++lno, len);

			/* If we hit EOF, stay there (historic practice). */
			if (p == NULL) {
				/* If already at eof, complain. */
				if (empty && !(vp->flags & VC_ISMOTION)) {
					v_eof(NULL);
					return (1);
				}
				--lno;
				EGETLINE(p, lno, len);
				rp->lno = lno;
				rp->cno = len ?
				    vp->flags & VC_ISMOTION ? len : len - 1 : 0;
				return (0);
			}
			cno = 0;
			startp = p;

			/* Skip leading space to first word. */
			FW(isspace(*p));
		}
	}
	rp->lno = lno;
	rp->cno = cno + (p - startp);
	return (0);
}

/*
 * v_wordb -- [count]b
 *	Move backward a word at a time.
 */
int
v_wordb(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (bword(vp, cp, rp, 0));
}

/*
 * v_WordB -- [count]B
 *	Move backward a bigword at a time.
 */
int
v_wordB(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (bword(vp, cp, rp, 1));
}

/*
 * bword --
 *	Move backward by words.
 */
static int
bword(vp, cp, rp, spaceonly)
	VICMDARG *vp;
	MARK *cp, *rp;
	int spaceonly;
{
	register char *p;
	size_t len;
	u_long cno, cnt, lno;
	char *startp;

	lno = cp->lno;
	cno = cp->cno;

	/* Check for start of file. */
	if (lno == 1 && cno == 0) {
		v_sof(NULL);
		return (1);
	}

	/* Get the line. */
	EGETLINE(p, lno, len);

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	/*
	 * Reset the length to the number of characters in the line; the
	 * first character is the current cursor position.
	 */
	len = cno ? cno + 1 : 0;
	if (len == 0)
		goto line;
	for (startp = p, p += cno; cnt--;) {
		if (spaceonly) {
			if (!isspace(*p)) {
				if (len < 2)
					goto line;
				--p;
				--len;
			}
			BW(isspace(*p));
			if (len)
				BW(!isspace(*p));
			else
				goto line;
		} else {
			if (!isspace(*p)) {
				if (len < 2)
					goto line;
				--p;
				--len;
			}
			BW(isspace(*p));
			if (len)
				if (inword(*p))
					BW(inword(*p));
				else
					BW(!isspace(*p) && !inword(*p));
			else
				goto line;
		}

		if (cnt && len == 0) {
			/* If we hit SOF, stay there (historic practice). */
line:			if (lno == 1) {
				rp->lno = 1;
				rp->cno = 0;
				return (0);
			}

			/*
			 * Get the line.  If the line is empty, decrement
			 * count and get another one.
			 */
			EGETLINE(p, --lno, len);
			if (len == 0) {
				if (--cnt == 0) {
					rp->lno = lno;
					rp->cno = 0;
					return (0);
				}
				goto line;
			}

			/*
			 * Set the cursor to the end of the line.  If the word
			 * at the end of this line has only a single character,
			 * we've already skipped over it.
			 */
			startp = p;
			if (len) {
				p += len - 1;
				if (len > 1 && !isspace(p[0]))
					if (inword(p[0])) {
						if (!inword(p[-1]))
							--cnt;
					} else if (!isspace(p[-1]) &&
					    !inword(p[-1]))
							--cnt;
			}
		} else {
			++p;
			++len;
		}
	}
	rp->lno = lno;
	rp->cno = p - startp;
	return (0);
}

/*
 * v_worde -- [count]e
 *	Move forward to the end of the word.
 */
int
v_worde(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (eword(vp, cp, rp, 0));
}

/*
 * v_wordE -- [count]E
 *	Move forward to the end of the bigword.
 */
int
v_wordE(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (eword(vp, cp, rp, 1));
}

/*
 * eword --
 *	Move forward to the end of the word.
 */
static int
eword(vp, cp, rp, spaceonly)
	VICMDARG *vp;
	MARK *cp, *rp;
	int spaceonly;
{
	register char *p;
	size_t len;
	u_long cno, cnt, lno;
	int empty;
	char *startp;

	lno = cp->lno;
	cno = cp->cno;

	EGETLINE(p, lno, len);

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	/*
	 * Reset the length; the first character is the current cursor
	 * position.  If no more characters in this line, may already
	 * be at EOF.
	 */
	len -= cno;
	if (empty = len == 1)
		goto line;

	for (startp = p += cno; cnt--; empty = 0) {
		if (spaceonly) {
			if (!isspace(*p)) {
				if (len < 2)
					goto line;
				++p;
				--len;
			}
			FW(isspace(*p));
			if (len)
				FW(!isspace(*p));
			else
				++cnt;
		} else {
			if (!isspace(*p)) {
				if (len < 2)
					goto line;
				++p;
				--len;
			}
			FW(isspace(*p));
			if (len)
				if (inword(*p))
					FW(inword(*p));
				else
					FW(!isspace(*p) && !inword(*p));
			else
				++cnt;
		}

		if (cnt && len == 0) {
line:			GETLINE(p, ++lno, len);

			/* If we hit EOF, stay there (historic practice). */
			if (p == NULL) {
				/* If already at eof, complain. */
				if (empty) {
					v_eof(NULL);
					return (1);
				}
				--lno;
				EGETLINE(p, lno, len);
				rp->lno = lno;
				rp->cno = len ? len - 1 : 0;
				return (0);
			}
			cno = 0;
			startp = p;
		} else {
			--p;
			++len;
		}
	}
	rp->lno = lno;
	rp->cno = cno + (p - startp);
	return (0);
}
