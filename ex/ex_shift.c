/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shift.c,v 5.15 1993/01/11 15:51:29 bostic Exp $ (Berkeley) $Date: 1993/01/11 15:51:29 $";
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
static int shift __P((EXCMDARG *, enum which));

int
ex_shiftl(cmdp)
	EXCMDARG *cmdp;
{
	return (shift(cmdp, LEFT));
}
	
int
ex_shiftr(cmdp)
	EXCMDARG *cmdp;
{
	return (shift(cmdp, RIGHT));
}

static int
shift(cmdp, rl)
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
		if ((p = file_gline(curf, from, &len)) == NULL)
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
		if (blen < newcol + len && binc(&buf, &blen, 0))
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
		bcopy(p + oldidx, bp, len - oldidx);

		/* Set the replacement line. */
		if (file_sline(curf, from, buf, (bp + (len - oldidx)) - buf)) {
err:			if (buf != NULL)
				free(buf);
			return (1);
		}
	}
	if (buf != NULL)
		free(buf);

	/* Reporting. */
	curf->rptlines = cmdp->addr2.lno - cmdp->addr1.lno + 1;
	curf->rptlabel = rl == RIGHT ? "shifted right" : "shifted left";

	FF_SET(curf, F_AUTOPRINT);
	return (0);
}
