/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_match.c,v 8.9 1994/03/08 19:41:19 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:41:19 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_match -- %
 *	Search to matching character.
 */
int
v_match(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	VCS cs;
	recno_t lno;
	size_t len, off;
	int cnt, matchc, startc, (*gc)__P((SCR *, EXF *, VCS *));
	char *p;

	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			goto nomatch;
		GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	/*
	 * !!!
	 * Historical practice was to search in the forward direction only.
	 */
	for (off = vp->m_start.cno;; ++off) {
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

	cs.cs_lno = vp->m_start.lno;
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

	vp->m_stop.lno = cs.cs_lno;
	vp->m_stop.cno = cs.cs_cno;

	/*
	 * If moving right, non-motion commands move to the end of the range.
	 * VC_D and VC_Y stay at the start.  If moving left, non-motion and
	 * VC_D commands move to the end of the range.  VC_Y remains at the
	 * start.  Ignore VC_C and VC_S.
	 *
	 * !!!
	 * Don't correct for leftward movement -- historic vi deleted the
	 * starting cursor position when deleting to a match.
	 */
	if (vp->m_start.cno > vp->m_stop.cno) {
		vp->m_final = vp->m_stop;
		if (ISMOTION(vp)) {
			if (F_ISSET(vp, VC_Y))
				vp->m_final = vp->m_start;
		}
	} else
		vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	return (0);
}
