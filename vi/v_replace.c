/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 5.12 1993/01/23 16:37:54 bostic Exp $ (Berkeley) $Date: 1993/01/23 16:37:54 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "term.h"

int
v_replace(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t cno, len;
	u_long cnt;
	u_char *np, *p, emptybuf[1];

	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) != 0) {
			GETLINE_ERR(fm->lno);
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
		v_eol(fm);
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
				msg("Error: %s", strerror(errno));
				return (1);
			}
			memmove(np, p, len);
			p = np;
		}
		for (; cnt--; ++lno, cno = 0) {
			if (file_sline(curf, lno, p, cno))
				goto err;
			p += cno + 1;
			len -= cno + 1;
			if (file_aline(curf, lno, p, len)) {
err:				if (p != emptybuf)
					free(np);
				return (1);
			}
		}
		break;
	default:
		if ((np = malloc(len)) == NULL) {
			msg("Error: %s", strerror(errno));
			return (1);
		}
		memmove(np, p, len);
		memset(np + fm->cno, vp->character, cnt);
		if (file_sline(curf, fm->lno, np, len)) {
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
