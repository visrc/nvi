/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 5.8 1992/12/05 11:10:50 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:10:50 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "search.h"
#include "getc.h"
#include "options.h"

static int	findmatchc __P((MARK *, u_char *, size_t, MARK *));

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
	u_char *p;

	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) == 0)
			goto nomatch;
		GETLINE_ERR(fm->lno);
		return (1);
	}

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

	if (getc_init(fm, &ch))
		return (1);
	for (cnt = 1; getc_next(dir, &ch);)
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
	u_char *p;
	size_t len;
{
	register size_t off;
	size_t left, right;			/* Can't be uninitialized. */
	int leftfound, rightfound;
	u_char *t;

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
