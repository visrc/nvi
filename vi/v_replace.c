/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 5.14 1993/02/24 12:58:59 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:58:59 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "term.h"
#include "vcmd.h"

int
v_replace(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t cno, len;
	u_long cnt;
	u_char *np, *p, emptybuf[1];

	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) != 0) {
			GETLINE_ERR(ep, fm->lno);
			return (1);
		}
		p = emptybuf;
		len = 1;
	} else if (len == 0) {
		p = emptybuf;
		len = 1;
	}

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	rp->cno = fm->cno + cnt - 1;
	if (rp->cno > len - 1) {
		v_eol(ep, fm);
		return (1);
	}

	/*
	 * The r command in historic vi was, for lack of a better word, wrong.
	 * "r<erase>" and "r<word erase>" beeped the terminal and deleted a
	 * single character.  "Nr<carriage return>" where N was greater than 1
	 * inserted a single carriage return.
	 */
	switch(special[vp->character]) {
	case K_ESCAPE:
		*rp = *fm;
		break;
	case K_CR:
	case K_NL:
		lno = fm->lno;
		cno = fm->cno;

		rp->lno = lno + cnt;
		rp->cno = 0;

		if (p != emptybuf) {
			if ((np = malloc(len)) == NULL) {
				msg(ep, M_ERROR, "Error: %s", strerror(errno));
				return (1);
			}
			memmove(np, p, len);
			p = np;
		}
		for (; cnt--; ++lno, cno = 0) {
			if (file_sline(ep, lno, p, cno))
				goto err;
			p += cno + 1;
			len -= cno + 1;
			if (file_aline(ep, lno, p, len)) {
err:				if (p != emptybuf)
					free(np);
				return (1);
			}
		}
		break;
	default:
		if ((np = malloc(len)) == NULL) {
			msg(ep, M_ERROR, "Error: %s", strerror(errno));
			return (1);
		}
		memmove(np, p, len);
		memset(np + fm->cno, vp->character, cnt);
		if (file_sline(ep, fm->lno, np, len)) {
			free(np);
			return (1);
		}
		free(np);

		/* If a count, move to the end, otherwise don't move. */
		rp->lno = fm->lno;
		if (cnt == 1)
			rp->cno = fm->cno;
		else
			rp->cno = fm->cno + cnt - 1;
		break;
	}
	return (0);
}
