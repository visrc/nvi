/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shift.c,v 5.17 1993/02/16 20:10:24 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:24 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"

enum which {LEFT, RIGHT};
static int shift __P((EXF *, EXCMDARG *, enum which));

int
ex_shiftl(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (shift(ep, cmdp, LEFT));
}
	
int
ex_shiftr(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (shift(ep, cmdp, RIGHT));
}

static int
shift(ep, cmdp, rl)
	EXF *ep;
	EXCMDARG *cmdp;
	enum which rl;
{
	recno_t from, to;
	size_t blen, len, newcol, oldcol, oldidx;
	u_char *p, *buf, *bp;

	if (LVAL(O_SHIFTWIDTH) == 0)
		return (0);

	blen = 0;
	buf = NULL;
	for (from = cmdp->addr1.lno, to = cmdp->addr2.lno; from <= to; ++from) {
		/* Get the line. */
		if ((p = file_gline(ep, from, &len)) == NULL)
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
				oldcol +=
				    LVAL(O_TABSTOP) - oldcol % LVAL(O_TABSTOP);
			else
				break;

		/* Calculate the new indent amount. */
		if (rl == RIGHT)
			newcol = oldcol + LVAL(O_SHIFTWIDTH);
		else {
			newcol = oldcol < LVAL(O_SHIFTWIDTH) ?
			    0 : oldcol - LVAL(O_SHIFTWIDTH);
			if (newcol == oldcol)
				continue;
		}

		/* Get a buffer that will hold the new line. */
		if (blen < newcol + len && binc(ep, &buf, &blen, 0))
			goto err;

		/* Build a new indent string. */
		bp = buf;
		if (ISSET(O_AUTOTAB))
			while (newcol >= LVAL(O_TABSTOP)) {
				*bp++ = '\t';
				newcol -= LVAL(O_TABSTOP);
			}

		for (; newcol > 0; --newcol)
			*bp++ = ' ';

		/* Add the original line. */
		memmove(bp, p + oldidx, len - oldidx);

		/* Set the replacement line. */
		if (file_sline(ep, from, buf, (bp + (len - oldidx)) - buf)) {
err:			if (buf != NULL)
				free(buf);
			return (1);
		}
	}
	if (buf != NULL)
		free(buf);

	/* Reporting. */
	ep->rptlines = cmdp->addr2.lno - cmdp->addr1.lno + 1;
	ep->rptlabel = rl == RIGHT ? "shifted right" : "shifted left";

	FF_SET(ep, F_AUTOPRINT);
	return (0);
}
