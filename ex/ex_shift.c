/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shift.c,v 5.9 1992/05/15 11:07:30 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:07:30 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

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
	MARK fm, tm;
	u_long flno, tlno;
	size_t len;
	int oldidx;		/* Number of chars used for indent. */
	int newidx;		/* Number of chars in the new indent string. */
	int oldcol;		/* Previous indent amount. */
	int newcol;		/* New indent amount. */
	char *p, sbuf[100];

	flno = cmdp->addr1.lno;
	tlno = cmdp->addr2.lno;
	for (; flno < tlno; ++flno) {

		/* Get the line. */
		if ((p = file_gline(curf, flno, &len)) == NULL)
			return (1);
		if (!len)
			continue;

		/* Calculate oldidx and oldcol. */
		for (oldidx = 0, oldcol = 0;
		    p[oldidx] == ' ' || p[oldidx] == '\t'; oldidx++)
			if (p[oldidx] == ' ')
				oldcol += 1;
			else
				oldcol +=
				    LVAL(O_TABSTOP) - oldcol % LVAL(O_TABSTOP);

		/* Calculate newcol. */
		if (rl == RIGHT)
			newcol = oldcol + (LVAL(O_SHIFTWIDTH) & 0xff);
			if (newcol == oldcol)
				continue;
		else {
			newcol = oldcol - (LVAL(O_SHIFTWIDTH) & 0xff);
			if (newcol < 0)
				newcol = 0;
			if (newcol == oldcol)
				continue;
		}

		if (newcol > sizeof(sbuf) - 1) {
			msg("Shift too large.");
			return (1);
		}

		/* Build a new indent string. */
		newidx = 0;
		if (ISSET(O_AUTOTAB))
			while (newcol >= LVAL(O_TABSTOP)) {
				sbuf[newidx++] = '\t';
				newcol -= LVAL(O_TABSTOP);
			}

		for (; newcol > 0; --newcol)
			sbuf[newidx++] = ' ';

		/* Change the old indent string into the new. */
		fm.lno = tm.lno = flno;
		fm.cno = 0;
		tm.cno = oldidx;
		change(&fm, &tm, sbuf, newidx);
	}

	/* Reporting. */
	rptlines = cmdp->addr2.lno - cmdp->addr1.lno + 1;
	rptlabel = rl == RIGHT ? "shifted right" : "shifted left";

	autoprint = 1;

	return (0);
}
