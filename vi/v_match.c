/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 5.3 1992/06/07 17:53:53 bostic Exp $ (Berkeley) $Date: 1992/06/07 17:53:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

static int	findmatchc __P((MARK *, char *, size_t, MARK *));
static int	getc_init __P((MARK *, enum direction));
static int	getc_next __P((int *));
static void	getc_set __P((MARK *));

/*
 * v_match -- %
 *	Search to matching character.
 */
int
v_match(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int cnt, matchc, startc;
	enum direction dir;
	size_t len;
	int ch;
	char *p;

	EGETLINE(p, fm->lno, len);
	if (len == 0)
		goto nomatch;

	switch(startc = p[fm->cno]) {
	case '(':
		matchc = ')';
		dir = FORWARD;
		break;
	case ')':
		matchc = '(';
		dir = BACKWARD;
		break;
	case '[':
		matchc = ']';
		dir = FORWARD;
		break;
	case ']':
		matchc = '[';
		dir = BACKWARD;
		break;
	case '{':
		matchc = '}';
		dir = FORWARD;
		break;
	case '}':
		matchc = '{';
		dir = BACKWARD;
		break;
	default:
		if (findmatchc(fm, p, len, rp)) {
nomatch:		bell();
			if (ISSET(O_VERBOSE))
				msg("No match character on this line.");
			return (1);
		}
		return (0);
	}

	if (getc_init(fm, dir))
		return (1);
	for (cnt = 1; getc_next(&ch);)
		if (ch == matchc) {
			if (--cnt == 0) {
				getc_set(rp);
				return (0);
			}
		} else if (ch == startc) {
			++cnt;
			continue;
		}

	bell();
	if (ISSET(O_VERBOSE))
		msg("Matching character not found.");
	return (1);
}

/*
 * findmatchc --
 *	If we're not on a character we know how to match, try and find the
 *	closest one on the line.  Vi used to do this, but in an inexplicable
 *	manner.  We search forward, then backward, and go to the closest one.
 *	Ties go to the left for no reason.
 */
static int
findmatchc(fm, p, len, rp)
	MARK *fm, *rp;
	char *p;
	size_t len;
{
	register size_t off;
	size_t left, right;
	int leftfound, rightfound;
	char *t;

	leftfound = rightfound = 0;
	for (off = 0, t = &p[off]; off++ < fm->cno;)
		if (index("{}[]()", *t++)) {
			left = off - 1;
			leftfound = 1;
			break;
		}

	--len;
	for (off = fm->cno + 1, t = &p[off]; off++ < len;)
		if (index("{}[]()", *t++)) {
			right = off - 1;
			rightfound = 1;
			break;
		}

	rp->lno = fm->lno;
	if (leftfound)
		if (rightfound)
			if (fm->cno - left > right - fm->cno)
				rp->cno = right;
			else
				rp->cno = left;
		else
			rp->cno = left;
	else if (rightfound)
		rp->cno = right;
	else
		return (1);
	return (0);
}

static enum direction getc_dir;
static MARK getc_m;
static char *getc_p;
static size_t getc_len;

static int
getc_init(fm, dir)
	MARK *fm;
	enum direction dir;
{
	getc_m = *fm;
	getc_dir = dir;
	EGETLINE(getc_p, fm->lno, getc_len);
	return (0);
}

static int
getc_next(chp)
	int *chp;
{
	if (getc_dir == FORWARD)
		if (getc_m.cno == getc_len) {
			for (;;) {
				GETLINE(getc_p, ++getc_m.lno, getc_len);
				if (getc_p == NULL)
					return (0);
				if (getc_len != 0)
					break;
			}
			getc_m.cno = 0;
		} else
			++getc_m.cno;
	else /* if (getc_dir == BACKWARD) */
		if (getc_m.cno == 0) {
			for (;;) {
				GETLINE(getc_p, --getc_m.lno, getc_len);
				if (getc_p == NULL)
					return (0);
				if (getc_len != 0)
					break;
			}
			getc_m.cno = getc_len - 1;
		} else
			--getc_m.cno;
	*chp = getc_p[getc_m.cno];
	return (1);
}

static void
getc_set(rp)
	MARK *rp;
{
	*rp = getc_m;
}
