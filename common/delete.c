/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: delete.c,v 5.11 1992/10/26 17:44:14 bostic Exp $ (Berkeley) $Date: 1992/10/26 17:44:14 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "vi.h"
#include "exf.h"
#include "extern.h"

/*
 * delete --
 *	Delete a range of text.
 */
int
delete(ep, fm, tm, lmode)
	EXF *ep;
	MARK *fm, *tm;
	int lmode;
{
	recno_t lno;
	size_t len, tlen;
	u_char *bp, *p;
	int eof;

#if DEBUG && 1
	TRACE("delete: from %lu/%d to %lu/%d\n",
	    fm->lno, fm->cno, tm->lno, tm->cno);
#endif
	/* Case 1 -- delete in line mode. */
	if (lmode) {
		for (lno = tm->lno; lno >= fm->lno; --lno)
			if (file_dline(ep, lno))
				return (1);
		goto done;
	}

	/*
	 * Case 2 -- delete to EOF.  This is a special case because it's
	 * easier to pick it off than try and find it in the other cases.
 	 */
	lno = file_lline(ep);
	if (tm->lno >= lno) {
		if (tm->lno == lno) {
			if ((p = file_gline(ep, lno, &len)) == NULL) {
				GETLINE_ERR(lno);
				return (1);
			}
			eof = tm->cno > len ? 1 : 0;
		} else
			eof = 1;
		if (eof) {
			for (lno = tm->lno; lno > fm->lno; --lno)
				if (file_dline(ep, lno))
					return (1);
			if (fm->cno == 0) {
				if (file_dline(ep, fm->lno))
					return (1);
				goto done;
			}
			if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			if (file_sline(ep, fm->lno, p, fm->cno))
				return (1);
			goto done;
		}
	}

	/* Case 3 -- delete within a single line. */
	if (tm->lno == fm->lno) {
		if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
			GETLINE_ERR(fm->lno);
			return (1);
		}
		bcopy(p + tm->cno,
		    p + fm->cno, len - fm->cno - (tm->cno - fm->cno));
		len -= tm->cno - fm->cno;
		if (file_sline(ep, fm->lno, p, len))
			return (1);
		goto done;
	}

	/* Case 4 -- delete over multiple lines. */

	/* Delete all the intermediate lines. */
	for (lno = tm->lno - 1; lno > fm->lno; --lno)
		if (file_dline(ep, lno))
			return (1);

	/* Figure out how big a buffer we need. */
	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		GETLINE_ERR(fm->lno);
		return (1);
	}
	tlen = len;
	if ((p = file_gline(ep, tm->lno, &len)) == NULL) {
		GETLINE_ERR(tm->lno);
		return (1);
	}
	tlen += len;		/* XXX Possible overflow! */
	if ((bp = malloc(tlen)) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}

	/* Copy the start partial line into place. */
	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		GETLINE_ERR(fm->lno);
		return (1);
	}
	bcopy(p, bp, fm->cno);
	tlen = fm->cno;

	/* Copy the end partial line into place. */
	if ((p = file_gline(ep, tm->lno, &len)) == NULL) {
		GETLINE_ERR(tm->lno);
		return (1);
	}
	bcopy(p + tm->cno, bp + tlen, len - tm->cno);
	tlen += len - tm->cno;

	/* Set the current line. */
	if (file_sline(ep, fm->lno, bp, tlen)) {
		free(bp);
		return (1);
	}

	/* Delete the new number of the old last line. */
	if (file_dline(ep, fm->lno + 1)) {
		free(bp);
		return (1);
	}

	/* Update the marks. */
done:	mark_delete(fm, tm, lmode);

	/*
	 * Reporting.
 	 * XXX
	 * Wrong, if deleting characters.
	 */
	rptlines = tm->lno - fm->lno + 1;
	rptlabel = "deleted";

	return (0);
}
