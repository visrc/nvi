/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 5.1 1992/05/15 11:15:06 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:15:06 $";
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
v_match(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK *fmp;
	regexp *re;
	size_t len;
	char *p, *ptrn;
	int cnt, found, startc;

	EGETLINE(p, cp->lno, len);

	switch(startc = p[cp->cno]) {
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
		if (regexec(re, p + cp->cno, 0) == 1) {
			found = 1;
			rp->cno = re->startp[0] - p;
		}
		if (regexec(re, p, 1) == 1 && re->startp[0] - p < cp->cno) {
			if (!found ||
			    rp->cno < re->startp[0] - p - cp->cno) {
				found = 1;
				rp->cno = re->startp[0] - p;
			}
		}
		if (found == 0) {
nomatch:		bell();
			msg("No match character on line %lu.", cp->lno);
			return (1);
		}
		rp->lno = cp->lno;
		return (0);
	}

	fmp = cp;
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
