/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: delete.c,v 8.8 1994/02/26 17:22:17 bostic Exp $ (Berkeley) $Date: 1994/02/26 17:22:17 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

/*
 * delete --
 *	Delete a range of text.
 */
int
delete(sp, ep, fm, tm, lmode)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm;
	int lmode;
{
	recno_t lno;
	size_t blen, len, tlen;
	char *bp, *p;
	int eof;

	bp = NULL;

	/* Case 1 -- delete in line mode. */
	if (lmode) {
		for (lno = tm->lno; lno >= fm->lno; --lno)
			if (file_dline(sp, ep, lno))
				return (1);
		goto vdone;
	}

	/*
	 * Case 2 -- delete to EOF.  This is a special case because it's
	 * easier to pick it off than try and find it in the other cases.
 	 */
	if (file_lline(sp, ep, &lno))
		return (1);
	if (tm->lno >= lno) {
		if (tm->lno == lno) {
			if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			eof = tm->cno >= len ? 1 : 0;
		} else
			eof = 1;
		if (eof) {
			for (lno = tm->lno; lno > fm->lno; --lno) {
				if (file_dline(sp, ep, lno))
					return (1);
				++sp->rptlines[L_DELETED];
			}
			if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
				GETLINE_ERR(sp, fm->lno);
				return (1);
			}
			GET_SPACE_RET(sp, bp, blen, fm->cno);
			memmove(bp, p, fm->cno);
			if (file_sline(sp, ep, fm->lno, bp, fm->cno))
				return (1);
			goto done;
		}
	}

	/* Case 3 -- delete within a single line. */
	if (tm->lno == fm->lno) {
		if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		GET_SPACE_RET(sp, bp, blen, len);
		if (fm->cno != 0)
			memmove(bp, p, fm->cno);
		memmove(bp + fm->cno, p + (tm->cno + 1), len - (tm->cno + 1));
		if (file_sline(sp, ep, fm->lno,
		    bp, len - ((tm->cno - fm->cno) + 1)))
			goto err;
		goto done;
	}

	/*
	 * Case 4 -- delete over multiple lines.
	 *
	 * Figure out how big a buffer we need.
	 */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}
	tlen = len;
	if ((p = file_gline(sp, ep, tm->lno, &len)) == NULL) {
		GETLINE_ERR(sp, tm->lno);
		return (1);
	}

	/*
	 * XXX
	 * We can overflow memory here, if (len + tlen) > SIZE_T_MAX.  The
	 * only portable way I've found to test is to depend on the overflow
	 * being less than the value.
	 */
	tlen += len;
	if (len > tlen) {
		msgq(sp, M_ERR, "Error: line length overflow");
		return (1);
	}

	GET_SPACE_RET(sp, bp, blen, tlen);

	/* Copy the start partial line into place. */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		GETLINE_ERR(sp, fm->lno);
		goto err;
	}
	memmove(bp, p, fm->cno);
	tlen = fm->cno;

	/* Copy the end partial line into place. */
	if ((p = file_gline(sp, ep, tm->lno, &len)) == NULL) {
		GETLINE_ERR(sp, tm->lno);
		goto err;
	}
	memmove(bp + tlen, p + (tm->cno + 1), len - (tm->cno + 1));
	tlen += len - (tm->cno + 1);

	/* Set the current line. */
	if (file_sline(sp, ep, fm->lno, bp, tlen))
		goto err;

	/* Delete the last and intermediate lines. */
	for (lno = tm->lno; lno > fm->lno; --lno)
		if (file_dline(sp, ep, lno))
			return (1);

	/* Reporting. */
vdone:	sp->rptlines[L_DELETED] += tm->lno - fm->lno + 1;

done:	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);

	return (0);

	/* Free memory. */
err:	FREE_SPACE(sp, bp, blen);

	return (1);
}
