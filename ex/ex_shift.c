/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shift.c,v 5.24 1993/05/09 09:21:22 bostic Exp $ (Berkeley) $Date: 1993/05/09 09:21:22 $";
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
	size_t blen, len, newcol, newidx, oldcol, oldidx;
	char *p, *buf, *bp;

	if (O_VAL(sp, O_SHIFTWIDTH) == 0) {
		msgq(sp, M_INFO, "shiftwidth option set to 0");
		return (0);
	}

	blen = 0;
	buf = NULL;
	for (from = cmdp->addr1.lno, to = cmdp->addr2.lno; from <= to; ++from) {
		if ((p = file_gline(sp, ep, from, &len)) == NULL)
			goto err;
		if (!len)
			continue;

		/*
		 * Calculate the old number of characters used for the indent
		 * and the previous indent amount.
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
			newcol = oldcol + O_VAL(sp, O_SHIFTWIDTH);
		else {
			newcol = oldcol < O_VAL(sp, O_SHIFTWIDTH) ?
			    0 : oldcol - O_VAL(sp, O_SHIFTWIDTH);
			if (newcol == oldcol)
				continue;
		}

		/* Get a buffer that will hold the new line. */
		if (blen < newcol + len && binc(sp, &buf, &blen, 0))
			goto err;

		/* Build a new indent string. */
		for (bp = buf, newidx = 0;
		    newcol >= O_VAL(sp, O_TABSTOP); ++newidx) {
			*bp++ = '\t';
			newcol -= O_VAL(sp, O_TABSTOP);
		}
		for (; newcol > 0; --newcol, ++newidx)
			*bp++ = ' ';

		/* Add the original line. */
		memmove(bp, p + oldidx, len - oldidx);

		/* Set the replacement line. */
		if (file_sline(sp, ep,
		    from, buf, (bp + (len - oldidx)) - buf)) {
err:			if (buf != NULL)
				free(buf);
			return (1);
		}

		/* Adjust the cursor, if necessary. */
		if (sp->lno == from)
			if (newidx > oldidx)
				sp->cno += newidx - oldidx;
			else
				sp->cno -= oldidx - newidx;
	}
	if (buf != NULL)
		free(buf);

	/* Reporting. */
	sp->rptlines = cmdp->addr2.lno - cmdp->addr1.lno + 1;
	sp->rptlabel = rl == RIGHT ? "shifted right" : "shifted left";

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
