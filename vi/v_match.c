/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 8.7 1993/12/09 17:05:51 bostic Exp $ (Berkeley) $Date: 1993/12/09 17:05:51 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "vcmd.h"

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
	size_t len, off;
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

	/*
	 * !!!
	 * Historical practice was to search in the forward direction only.
	 */
	for (off = fm->cno;; ++off) {
		if (off >= len) {
nomatch:		msgq(sp, M_BERR, "No match character on this line.");
			return (1);
		}
		switch (startc = p[off]) {
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
			continue;
		}
		break;
	}

	cs.cs_lno = fm->lno;
	cs.cs_cno = off;
	if (cs_init(sp, ep, &cs))
		return (1);
	for (cnt = 1;;) {
		if (gc(sp, ep, &cs))
			return (1);
		if (cs.cs_flags != 0) {
			if (cs.cs_flags == CS_EOF || cs.cs_flags == CS_SOF)
				break;
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
	 * MARK or from MARK depending on the direction of the search.
	 */
	if (F_ISSET(vp, VC_C | VC_D | VC_Y)) {
		if (file_gline(sp, ep, rp->lno, &len) == NULL) {
			GETLINE_ERR(sp, rp->lno);
			return (1);
		}
		if (len)
			if (gc == cs_next)
				++rp->cno;
			else
				++fm->cno;
	}
	return (0);
}
