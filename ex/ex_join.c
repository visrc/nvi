/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_join.c,v 5.19 1993/02/28 14:00:36 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:36 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_join -- :[line [,line]] j[oin][!] [count] [flags]
 *	Join lines.
 */
int
ex_join(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t from, to;
	size_t blen, clen, len, tlen;
	int echar, first;
	u_char *bp, *buf, *p;

	from = cmdp->addr1.lno;
	to = cmdp->addr2.lno;

	/* Check for no lines to join. */
	if ((p = file_gline(ep, from + 1, &len)) == NULL) {
		ep->msg(ep, M_ERROR, "No remaining lines to join.");
		return (1);
	}

	blen = tlen = 0;
        bp = buf = NULL;
        for (first = 1, from = cmdp->addr1.lno, to = cmdp->addr2.lno;
	    from <= to; ++from) {
		/*
		 * Get next line.  Historic versions of vi allowed "10J" while
		 * less than 10 lines from the end-of-file, so we do too.
		 */
		if ((p = file_gline(ep, from, &len)) == NULL)
			break;

		/* Empty lines just go away. */
		if (len == 0)
			continue;

		/*
		 * Get more space if necessary.  Note, tlen isn't the length
		 * of the new line, it's roughly the amount of space needed.
		 * Bp - buf is the length of the new line.
		 */
		tlen += len + 2;
		if (blen < tlen) {
			clen = bp == NULL ? 0 : bp - buf;
			if (binc(ep, &buf, &blen, tlen)) {
				if (buf != NULL)
					free(buf);
				return (1);
			}
			bp = buf + clen;
		}

		/*
		 * Historic practice:
		 *
		 * If force specified, join without modification.
		 * If the current line ends with whitespace, strip leading
		 *    whitespace from the joined line.
		 * If the next line starts with a ), do nothing.
		 * If the current line ends with ., ? or !, insert two spaces.
		 * Else, insert one space.
		 *
		 * Echar is the last character in the last line joined.
		 */
		if (!first && !(cmdp->flags & E_FORCE)) {
			if (isspace(echar))
				while (len-- && isspace(*p))
					++p;
			else if (p[0] != ')') {
				if (strchr(".?!", echar))
					*bp++ = ' ';
				*bp++ = ' ';
			}
		} else
			first = 0;

		if (len != 0) {
			memmove(bp, p, len);
			bp += len;
			echar = p[len - 1];
		} else
			echar = ' ';
	}

	/* Delete the joined lines. */
        for (from = cmdp->addr1.lno, to = cmdp->addr2.lno; to >= from; --to)
		if (file_dline(ep, to))
			goto err;
		
	/* Insert the new line into place. */
	if (file_aline(ep, cmdp->addr1.lno - 1, buf, bp - buf)) {
err:		if (buf != NULL)
			free(buf);
		return (1);
	}
	if (buf != NULL)
		free(buf);

	ep->rptlines = (cmdp->addr2.lno - cmdp->addr1.lno) + 1;
	ep->rptlabel = "joined";

	FF_SET(ep, F_AUTOPRINT);
	return (0);
}
