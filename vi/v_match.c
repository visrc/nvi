/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 5.20 1993/05/08 10:52:43 bostic Exp $ (Berkeley) $Date: 1993/05/08 10:52:43 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "vcmd.h"

static int	findmatchc __P((MARK *, char *, size_t, MARK *));

/*
 * v_match -- %
 *	Search to matching character.
 */
int
v_match(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int cnt, matchc, startc;
	enum direction dir;
	size_t len;
	int ch;
	char *p;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep) == 0)
			goto nomatch;
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		goto nomatch;

	switch (startc = p[fm->cno]) {
	case '/':
		matchc = '*';
		if (fm->cno < len - 1 && p[fm->cno + 1] == '*') {
			++fm->cno;
			dir = FORWARD;
			break;
		}
		if (fm->cno > 0 && p[fm->cno - 1] == '*') {
			--fm->cno;
			dir = BACKWARD;
			break;
		}
		goto search;
	case '*':
		startc = '/';			/* Ignore startc. */
		matchc = '*';
		if (fm->cno < len - 1 && p[fm->cno + 1] == '/') {
			dir = BACKWARD;
			break;
		}
		if (fm->cno > 0 && p[fm->cno - 1] == '/') {
			dir = FORWARD;
			break;
		}
		goto search;
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
search:		if (F_ISSET(vp, VC_C | VC_D | VC_Y)) {
			msgq(sp, M_BERR,
			"Proximity search doesn't work for motion commands.");
			return (1);
		}
		if (findmatchc(fm, p, len, rp)) {
nomatch:		msgq(sp, M_BERR, "No match character on this line.");
			return (1);
		}
		return (0);
	}

	if (getc_init(sp, ep, fm, &ch))
		return (1);
	for (cnt = 1; getc_next(sp, ep, dir, &ch);)
		if (ch == startc) {
			if (startc != '/')
				++cnt;
		} else if (ch == matchc) {
			if (ch == '*') {
				if (!getc_next(sp, ep, dir, &ch))
					break;
				if (ch != '/')
					continue;
			}
			if (--cnt == 0)
				break;
		} 
	if (cnt) {
		msgq(sp, M_BERR, "Matching character not found.");
		return (1);
	}
	getc_set(sp, ep, rp);

	/* Movement commands go one space further. */
	if (F_ISSET(vp, VC_C | VC_D | VC_Y)) {
		if (file_gline(sp, ep, rp->lno, &len) == NULL) {
			GETLINE_ERR(sp, rp->lno);
			return (1);
		}
		if (len)
			++rp->cno;
	}
	return (0);
}

/*
 * findmatchc --
 *	If we're not on a character we know how to match, try and find the
 *	closest character from the set "{}[]()".  (It might be useful to be
 *	able to enter % in the middle of the comment and find a match, but
 *	that would require searching across larger areas.)  The historic vi
 *	also looked for a character to match against, but in a completely
 *	inexplicable and apparently random fashion.  We search forward, then
 *	backward, and go to the closest one.  Ties go left for no reason.
 */
static int
findmatchc(fm, p, len, rp)
	MARK *fm, *rp;
	char *p;
	size_t len;
{
	register size_t off;
	size_t left, right;			/* Can't be uninitialized. */
	int leftfound, rightfound;
	char *t;

	leftfound = rightfound = 0;
	for (off = 0, t = &p[off]; off++ < fm->cno; ++t)
		if (strchr("/*{}[]()", t[0])) {
			if (t[0] == '/') {
				if (off == fm->cno || t[1] != '*')
					continue;
				left = off;
			} else if (t[0] == '*') {
				if (off == fm->cno || t[1] != '/')
					continue;
				left = off;
			} else
				left = off - 1;
			leftfound = 1;
			break;
		}

	--len;
	for (off = fm->cno + 1, t = &p[off]; off++ < len; ++t)
		if (strchr("/*{}[]()", t[0])) {
			if (t[0] == '/') {
				if (off == len || t[1] != '*')
					continue;
				right = off;
			} else if (t[0] == '*') {
				if (off == fm->cno || t[1] != '/')
					continue;
				right = off;
			} else
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
