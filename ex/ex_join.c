/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_join.c,v 5.26 1993/05/09 10:25:27 bostic Exp $ (Berkeley) $Date: 1993/05/09 10:25:27 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_join -- :[line [,line]] j[oin][!] [count] [flags]
 *	Join lines.
 */
int
ex_join(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	GS *gp;
	recno_t from, to;
	size_t blen, clen, len, tlen;
	int echar, first;
	char *bp, *p, *tbp;

	from = cmdp->addr1.lno;
	to = cmdp->addr2.lno;

	/* Check for no lines to join. */
	if ((p = file_gline(sp, ep, from + 1, &len)) == NULL) {
		msgq(sp, M_ERR, "No remaining lines to join.");
		return (1);
	}

	/* Get temp space. */
	gp = sp->gp;
	if (F_ISSET(gp, G_TMP_INUSE)) {
		bp = NULL;
		blen = 0;
	} else {
		bp = gp->tmp_bp;
		F_SET(gp, G_TMP_INUSE);
	}

	clen = tlen = 0;
        for (first = 1, from = cmdp->addr1.lno,
	    to = cmdp->addr2.lno + 1; from <= to; ++from) {
		/*
		 * Get next line.  Historic versions of vi allowed "10J" while
		 * less than 10 lines from the end-of-file, so we do too.
		 */
		if ((p = file_gline(sp, ep, from, &len)) == NULL)
			break;

		/* Empty lines just go away. */
		if (len == 0)
			continue;

		/*
		 * Get more space if necessary.  Note, tlen isn't the length
		 * of the new line, it's roughly the amount of space needed.
		 * tbp - bp is the length of the new line.
		 */
		tlen += len + 2;
		if (bp == gp->tmp_bp) {
			F_CLR(gp, G_TMP_INUSE);
			BINC(sp, gp->tmp_bp, gp->tmp_blen, tlen);
			bp = gp->tmp_bp;
			F_SET(gp, G_TMP_INUSE);
		} else
			BINC(sp, bp, blen, tlen);
		tbp = bp + clen;

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
		if (!first && !F_ISSET(cmdp, E_FORCE)) {
			if (isspace(echar))
				for (; len && isspace(*p); --len, ++p);
			else if (p[0] != ')') {
				if (strchr(".?!", echar))
					*tbp++ = ' ';
				*tbp++ = ' ';
				++clen;
				for (; len && isspace(*p); --len, ++p);
			}
		} else
			first = 0;
			
		if (len != 0) {
			memmove(tbp, p, len);
			tbp += len;
			clen += len;
			echar = p[len - 1];
		} else
			echar = ' ';

		/*
		 * Historic practice for vi was to put the cursor at the first
		 * inserted whitespace character, if there was one, or the
		 * first character of the joined line, if there wasn't.  If
		 * a count was specified, the cursor was moved as described
		 * for the first line joined, ignoring subsequent lines.  If
		 * the join was a ':' command, the cursor was placed at the
		 * first non-blank character of the line unless the cursor was
		 * "attracted" to the end of line when the command was executed
		 * in which case it moved to the new end of line.  There are
		 * probably several more special cases, but frankly, my dear,
		 * I don't give a damn.  This implementation puts the cursor
		 * on the first inserted whitespace character or the first
		 * character of the joined line, regardless.  Note, if the
		 * cursor isn't on the joined line (possible with : commands),
		 * it is reset to the starting line.
		 */
		sp->cno = (tbp - bp) - len - 1;
	}
	sp->lno = cmdp->addr1.lno;

	/* Delete the joined lines. */
        for (from = cmdp->addr1.lno, to = cmdp->addr2.lno + 1; to > from; --to)
		if (file_dline(sp, ep, to))
			goto err;
		
	/* Reset the original line. */
	if (file_sline(sp, ep, from, bp, tbp - bp)) {
err:		if (bp == gp->tmp_bp)
			F_CLR(gp, G_TMP_INUSE);
		else
			FREE(bp, blen);
		return (1);
	}
	if (bp == gp->tmp_bp)
		F_CLR(gp, G_TMP_INUSE);
	else
		FREE(bp, blen);

	sp->rptlines = (cmdp->addr2.lno - cmdp->addr1.lno) + 1;
	sp->rptlabel = "joined";

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
