/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_join.c,v 5.9 1992/05/15 11:08:33 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:08:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_join(cmdp)
	EXCMDARG *cmdp;
{
	MARK fm, tm;
	size_t blen, llen, tlen;
	u_long cnt, lno;
	int ret;
	char *bp, *p;

	fm = cmdp->addr1;
	tm = cmdp->addr2;

	/* If only one line is specified, join with the next one. */
	if (fm.lno == nlines) {
		msg("No remaining lines to join into this line.");
		return (1);
	}

	bp = NULL;
	blen = tlen = 0;

	lno = fm.lno;
	cnt = tm.lno - fm.lno;
	do {
		/* Get next line. */
		if ((p = file_gline(curf, lno, &llen)) == NULL)
			return (1);

		/* Resize buffer as necessary. */
		if (llen + tlen + 3 > blen) {
			blen = llen + tlen + 256;
			if ((bp = realloc(bp, blen)) == NULL) {
				msg("Error: %s", strerror(errno));
				return (1);
			}
		}

		/* If line ends with ., ?, or !, two spaces, otherwise one. */
		if (index(".?!", p[llen - 1]) == NULL)
			bp[tlen++] = ' ';
		bp[tlen++] = ' ';

		/* Skip leading space. */
		for (; *p && isspace(*p); ++p, --llen);

		/* Catenate. */
		bcopy(p, bp + tlen, llen);
		tlen += llen;

	} while (cnt-- > 1);

	ret = change(&fm, &tm, bp, tlen);

	free(bp);

	if (ret)
		return (1);

	if ((rptlines = tm.lno - fm.lno) == 0)
		rptlines = 1;
	rptlabel = "joined";

	autoprint = 1;
	return (0);
}
