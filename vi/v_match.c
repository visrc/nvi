/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 5.2 1992/05/27 10:40:18 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:40:18 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "regexp.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_match -- %
 *	Search to matching character.
 */
int
v_match(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *fmp;
	regexp *re;
	size_t len;
	char *p, *ptrn;
	int cnt, found, startc;

	EGETLINE(p, fm->lno, len);

	switch(startc = p[fm->cno]) {
	case '(':
		cnt = 1;
		ptrn = "[()]";
		break;
	case ')':
		cnt = -1;
		ptrn = "[()]";
		break;
	case '[':
		cnt = 1;
		ptrn = "[\\[\\]]";
		break;
	case ']':
		cnt = -1;
		ptrn = "[\\[\\]]";
		break;
	case '{':
		cnt = 1;
		ptrn = "[{}]";
		break;
	case '}':
		cnt = -1;
		break;
	default:
		/*
		 * If we're not on a character we know how to match, try and
		 * find the closest one on the line.  Vi used to do this, but
		 * in a totally inexplicable manner.
		 */
		if (regcomp("[()\\[\\]{}]") == NULL)
			goto nomatch;
		found = 0;
		if (regexec(re, p + fm->cno, 0) == 1) {
			found = 1;
			rp->cno = re->startp[0] - p;
		}
		if (regexec(re, p, 1) == 1 && re->startp[0] - p < fm->cno) {
			if (!found ||
			    rp->cno < re->startp[0] - p - fm->cno) {
				found = 1;
				rp->cno = re->startp[0] - p;
			}
		}
		if (found == 0) {
nomatch:		bell();
			msg("No match character on line %lu.", fm->lno);
			return (1);
		}
		rp->lno = fm->lno;
		return (0);
	}

	fmp = fm;
	if (cnt > 0)
		while ((fmp = f_search(fmp, ptrn, NULL, 0)) != NULL) {
			EGETLINE(p, fmp->lno, len);
			if (p[fmp->cno] != startc) {
				if (--cnt == 0)
					break;
			} else
				++cnt;
		}
	else
		while ((fmp = b_search(fmp, ptrn, NULL, 0)) != NULL) {
			EGETLINE(p, fmp->lno, len);
			if (p[fmp->cno] != startc) {
				if (++cnt == 0)
					break;
			} else
				--cnt;
		}
	if (fmp == NULL)
		return (1);
	*rp = *fmp;
	return (0);
}
