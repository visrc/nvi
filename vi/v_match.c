/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 8.3 1993/08/22 09:59:10 bostic Exp $ (Berkeley) $Date: 1993/08/22 09:59:10 $";
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
	VCS cs;
	recno_t lno;
	size_t len;
	int (*gc)__P((SCR *, EXF *, VCS *));
	char *p;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			goto nomatch;
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		goto nomatch;

	switch (startc = p[fm->cno]) {
	case '(':
		matchc = ')';
		gc = cs_next;
		break;
	case ')':
		matchc = '(';
		gc = cs_prev;
		break;
	case '[':
		matchc = ']';
		gc = cs_next;
		break;
	case ']':
		matchc = '[';
		gc = cs_prev;
		break;
	case '{':
		matchc = '}';
		gc = cs_next;
		break;
	case '}':
		matchc = '{';
		gc = cs_prev;
		break;
	default:
		if (F_ISSET(vp, VC_C | VC_D | VC_Y)) {
			msgq(sp, M_BERR,
			    "No proximity match for motion commands.");
			return (1);
		}
		if (findmatchc(fm, p, len, rp)) {
nomatch:		msgq(sp, M_BERR, "No match character on this line.");
			return (1);
		}
		return (0);
	}

	cs.cs_lno = fm->lno;
	cs.cs_cno = fm->cno;
	if (cs_init(sp, ep, &cs))
		return (1);
	for (cnt = 1;;) {
		if (gc(sp, ep, &cs))
			return (1);
		if (cs.cs_flags != 0) {
			if (cs.cs_flags == CS_EOF || cs.cs_flags == CS_SOF)
				break;
			if (cs.cs_flags != 0)
				continue;
		}
		if (cs.cs_ch == startc)
			++cnt;
		else if (cs.cs_ch == matchc && --cnt == 0)
			break;
	}
	if (cnt) {
		msgq(sp, M_BERR, "Matching character not found.");
		return (1);
	}
	rp->lno = cs.cs_lno;
	rp->cno = cs.cs_cno;

	/*
	 * Movement commands go one space further.  Increment the return
	 * position or THE CURSOR depending on the direction of the search.
	 * Changing the cursor isn't very nice -- mea culpa.
	 *
	 */
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		if (gc == cs_next) {
			if (file_gline(sp, ep, rp->lno, &len) == NULL) {
				GETLINE_ERR(sp, rp->lno);
				return (1);
			}
			if (len)
				++rp->cno;
		} else {
			if (file_gline(sp, ep, rp->lno, &len) == NULL) {
				GETLINE_ERR(sp, rp->lno);
				return (1);
			}
			if (len)
				++sp->cno;
		}
	return (0);
}

/*
 * findmatchc --
 *	If we're not on a character we know how to match, try and find the
 *	closest character from the set "{}[]()".  The historic vi would also
 *	look for a character to match against, but in an inexplicable and
 *	apparently random fashion.  We search forward, then backward, and go
 *	to the closest one.  Ties go left because differently handed people
 *	have been traditionally discriminated against by our society.
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
	for (off = 0, t = &p[off]; off++ < fm->cno;)
		if (strchr("{}[]()", *t++)) {
			left = off - 1;
			leftfound = 1;
			break;
		}

	--len;
	for (off = fm->cno + 1, t = &p[off]; off++ < len;)
		if (strchr("{}[]()", *t++)) {
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
