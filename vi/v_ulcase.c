/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ulcase.c,v 5.11 1992/10/18 13:10:32 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:10:32 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "extern.h"

/*
 * v_ulcase -- [count]~
 *	This function toggles upper & lower case letters.  This command
 *	in historic vi ignored the count.  It really should have had an
 *	associated motion, but it's too late to change it now.
 */
int
v_ulcase(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int ch;
	register u_char *p;
	size_t cno, len;
	recno_t lno;
	u_long cnt;
	int change;
	u_char *start;

	lno = fm->lno;
	cno = fm->cno;

	if ((start = file_gline(curf, lno, &len)) == NULL) {
		if (file_lline(curf) == 0)
			v_eof(NULL);
		else
			GETLINE_ERR(lno);
		return (1);
	}

	p = start + cno;

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	for (change = 0; cnt--; ++cno) {
		if (cno == len) {
			if (change) {
				if (file_sline(curf, lno, start, len)) {
					rp->lno = lno;
					rp->cno = cno;
					return (1);
				}
			}
			if ((start = file_gline(curf, ++lno, &len)) == NULL) {
				rp->lno = --lno;
				rp->cno = len - 1;
				return (0);
			}
			change = 0;
			cno = 0;
			p = start;
		}
		ch = *p;
		if (islower(ch)) {
			*p++ = toupper(ch);
			change = 1;
		} else if (isupper(ch)) {
			*p++ = tolower(ch);
			change = 1;
		} else
			++p;
	}
	rp->lno = lno;
	rp->cno = cno;
	if (change) {
		if (file_sline(curf, lno, start, len))
			return (1);
	}
	return (0);
}
