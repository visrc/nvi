/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ulcase.c,v 5.8 1992/05/27 10:38:23 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:38:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>
#include <ctype.h>

#include "vi.h"
#include "vcmd.h"
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
	register char *p;
	size_t cno, len;
	recno_t lno;
	u_long cnt;
	int change;
	char *start;

	lno = fm->lno;
	cno = fm->cno;

	EGETLINE(start, lno, len);
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
				scr_lchange(lno);
			}
			GETLINE(start, ++lno, len);
			if (start == NULL) {
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
		scr_lchange(lno);
	}
	return (0);
}
