/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shift.c,v 5.29 1993/05/15 10:10:45 bostic Exp $ (Berkeley) $Date: 1993/05/15 10:10:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

enum which {LEFT, RIGHT};
static int shift __P((SCR *, EXF *, EXCMDARG *, enum which));

int
ex_shiftl(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (shift(sp, ep, cmdp, LEFT));
}
	
int
ex_shiftr(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (shift(sp, ep, cmdp, RIGHT));
}

static int
shift(sp, ep, cmdp, rl)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which rl;
{
	recno_t from, to;
	size_t blen, len, newcol, newidx, oldcol, oldidx, sw;
	int curset;
	char *p, *bp, *tbp;

	if (O_VAL(sp, O_SHIFTWIDTH) == 0) {
		msgq(sp, M_INFO, "shiftwidth option set to 0");
		return (0);
	}

	/*
	 * The historic version of vi permitted the user to string any number
	 * of '>' or '<' characters together, resulting in an indent of the
	 * appropriate levels.  There's a special hack in ex_cmd() so that
	 * cmdp->string points to the string of '>' or '<' characters, but it
	 * is NOT nul terminated.
	 *
	 * Q: What's the difference between the people adding features
	 *    to vi and the Girl Scouts?
	 * A: The Girl Scouts have mint cookies and adult supervision.
	 */
	for (p = cmdp->string, sw = 0;
	    *p && (*p == '>' || *p == '<'); ++p, sw += O_VAL(sp, O_SHIFTWIDTH));

	GET_SPACE(sp, bp, blen, 256);

	curset = 0;
	for (from = cmdp->addr1.lno, to = cmdp->addr2.lno; from <= to; ++from) {
		if ((p = file_gline(sp, ep, from, &len)) == NULL)
			goto err;
		if (!len) {
			if (sp->lno == from)
				curset = 1;
			continue;
		}

		/*
		 * Calculate the old indent amount and the number of
		 * characters it used.
		 */
		for (oldidx = 0, oldcol = 0;; ++oldidx)
			if (p[oldidx] == ' ')
				++oldcol;
			else if (p[oldidx] == '\t')
				oldcol += O_VAL(sp, O_TABSTOP) -
				    oldcol % O_VAL(sp, O_TABSTOP);
			else
				break;

		/* Calculate the new indent amount. */
		if (rl == RIGHT)
			newcol = oldcol + sw;
		else {
			newcol = oldcol < sw ? 0 : oldcol - sw;
			if (newcol == oldcol)
				continue;
		}

		/* Get a buffer that will hold the new line. */
		ADD_SPACE(sp, bp, blen, newcol + len);

		/*
		 * Build a new indent string and count the number of
		 * characters it uses.
		 */
		for (tbp = bp, newidx = 0;
		    newcol >= O_VAL(sp, O_TABSTOP); ++newidx) {
			*tbp++ = '\t';
			newcol -= O_VAL(sp, O_TABSTOP);
		}
		for (; newcol > 0; --newcol, ++newidx)
			*tbp++ = ' ';

		/* Add the original line. */
		memmove(tbp, p + oldidx, len - oldidx);

		/* Set the replacement line. */
		if (file_sline(sp, ep, from, bp, (tbp + (len - oldidx)) - bp)) {
err:			FREE_SPACE(sp, bp, blen);
			return (1);
		}

		/*
		 * The shift command in historic vi had the usual bizarre
		 * collection of cursor semantics.  If called from vi, the
		 * cursor was repositioned to the first non-blank character
		 * of the lowest numbered line shifted.  If called from ex,
		 * the cursor was repositioned to the first non-blank of the
		 * highest numbered line shifted.  Here, if the cursor isn't
		 * part of the set of lines that are moved, move it to the
		 * first non-blank of the last line shifted.  (This makes
		 * ":3>>" in vi work reasonably.) Otherwise, don't move it
		 * at all (this makes dot useful, for example, ">'a."). 
		 */
		if (sp->lno == from) {
			curset = 1;
			if (newidx > oldidx)
				sp->cno += newidx - oldidx;
			else
				sp->cno -= oldidx - newidx;
		}
	}
	if (!curset) {
		sp->lno = to;
		if (nonblank(sp, ep, to, &sp->cno))
			sp->cno = 0;
	}

	FREE_SPACE(sp, bp, blen);

	sp->rptlines[rl == RIGHT ? L_RSHIFT : L_LSHIFT] +=
	    cmdp->addr2.lno - cmdp->addr1.lno + 1;

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
