/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ulcase.c,v 5.22 1993/04/17 12:06:13 bostic Exp $ (Berkeley) $Date: 1993/04/17 12:06:13 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_ulcase -- [count]~
 *	This function toggles upper & lower case letters.  In historic
 *	vi, the count was ignored.  It would have been better if there
 *	had been an associated motion, but it's too late to change it
 *	now.
 */
int
v_ulcase(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	GS *gp;
	recno_t lno;
	size_t bplen, lcnt, len;
	u_long cnt;
	int ch, change, rval;
	char *bp, *p;

	/* Figure out what memory to use. */
	gp = sp->gp;
	if (F_ISSET(gp, G_TMP_INUSE)) {
		bp = NULL;
		bplen = 0;
	} else {
		bp = gp->tmp_bp;
		F_SET(gp, G_TMP_INUSE);
	}

	/*
	 * Historic vi didn't permit ~ to cross newline boundaries.
	 * I can think of no reason why it shouldn't, which at least
	 * lets you auto-repeat through a paragraph.
	 */
	rval = 0;
	for (change = -1, cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt;) {
		/* Get the line; EOF is an infinite sink. */
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			if (file_lline(sp, ep) >= fm->lno) {
				GETLINE_ERR(sp, fm->lno);
				rval = 1;
				break;
			}
			if (change == -1) {
				v_eof(sp, ep, NULL);
				return (1);
			}
			break;
		}

		/* Set current line number. */
		lno = fm->lno;

		/* Empty lines just decrement the count. */
		if (len == 0) {
			--cnt;
			++fm->lno;
			fm->cno = 0;
			change = 0;
			continue;
		}

		/* Get a copy of the line. */
		if (bp == gp->tmp_bp) {
			BINC(sp, gp->tmp_bp, gp->tmp_blen, len);
			bp = gp->tmp_bp;
		} else
			BINC(sp, bp, bplen, len);
		memmove(bp, p, len);

		/* Set starting pointer. */
		if (change == -1)
			p = bp + fm->cno;
		else
			p = bp;

		/*
		 * Figure out how many characters get changed in this
		 * line.  Set the final cursor column.
		 */
		if (fm->cno + cnt >= len) {
			lcnt = len - fm->cno;
			++fm->lno;
			fm->cno = 0;
		} else
			fm->cno += lcnt = cnt;
		cnt -= lcnt;

		/* Change the line. */
		for (change = 0; lcnt--; ++p) {
			ch = *(u_char *)p;
			if (islower(ch)) {
				*p = toupper(ch);
				change = 1;
			} else if (isupper(ch)) {
				*p = tolower(ch);
				change = 1;
			}
		}

		/* Update the line if necessary. */
		if (change && file_sline(sp, ep, lno, bp, len)) {
			rval = 1;
			break;
		}
	}

	/* If changed lines, could be on an illegal line. */
	if (fm->lno != lno && file_gline(sp, ep, fm->lno, &len) == NULL) {
		--fm->lno;
		fm->cno = len ? len - 1 : 0;
	}
	*rp = *fm;

	if (bp == gp->tmp_bp)
		F_CLR(gp, G_TMP_INUSE);
	else
		free(bp);
	return (rval);
}
