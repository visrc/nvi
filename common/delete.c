/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: delete.c,v 5.10 1992/10/18 13:02:30 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:02:30 $";
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
delete(fm, tm, lmode)
	MARK *fm, *tm;
	int lmode;
{
	MARK m;
	recno_t lno;
	size_t len, tlen;
	u_char *bp, *p;

#if DEBUG && 1
	TRACE("delete: from {%lu, %d}, to {%lu, %d}\n",
	    fm->lno, fm->cno, tm->lno, tm->cno);
#endif

	/*
	 * Delete in reverse order.  There are three cases: line mode, a
	 * delete inside a line, and a delete over multiple lines.
	 */
	if (lmode) {
		for (lno = tm->lno; lno >= fm->lno; --lno)
			if (file_dline(curf, lno))
				return (1);
	} else {
		/*
		 * If deleting to the start of a line, decrement the
		 * line number and reset the count to max column + 1.
		 */
		if (tm->cno == 0) {
			if ((p = file_gline(curf, --tm->lno, &len)) == NULL) {
				GETLINE_ERR(tm->lno);
				return (1);
			}
			tm->cno = len;
		}
		/* If deleting within a single line, it's easy. */
		if (tm->lno == fm->lno) {
			if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			bcopy(p + tm->cno, p + fm->cno,
			    len - fm->cno - (tm->cno - fm->cno));
			len -= tm->cno - fm->cno;
			if (file_sline(curf, fm->lno, p, len))
				return (1);
		} else {
			/* Delete all the intermediate lines. */
			for (lno = tm->lno - 1; lno > fm->lno; --lno)
				if (file_dline(curf, lno))
					return (1);

			/* Figure out how big a buffer we need. */
			if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			tlen = len;
			if ((p = file_gline(curf, tm->lno, &len)) == NULL) {
				GETLINE_ERR(tm->lno);
				return (1);
			}
			tlen += len;		/* XXX Possible overflow! */
			if ((bp = malloc(tlen)) == NULL) {
				msg("Error: %s", strerror(errno));
				return (1);
			}

			/* Copy the start partial line into place. */
			if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
				GETLINE_ERR(fm->lno);
				return (1);
			}
			bcopy(p, bp, fm->cno);
			tlen = fm->cno;

			/* Copy the end partial line into place. */
			if ((p = file_gline(curf, tm->lno, &len)) == NULL) {
				GETLINE_ERR(tm->lno);
				return (1);
			}
			bcopy(p + tm->cno, bp + tlen, len - tm->cno);
			tlen += len - tm->cno;

			/* Set the current line. */
			if (file_sline(curf, fm->lno, bp, tlen)) {
				free(bp);
				return (1);
			}

			/* Delete the new number of the old last line. */
			if (file_dline(curf, fm->lno + 1)) {
				free(bp);
				return (1);
			}
		}
	}

	/* Update the marks. */
	mark_delete(fm, tm, lmode);

	/* Mark the file as modified. */
	curf->flags |= F_MODIFIED;

	/*
	 * Reporting.
 	 * XXX
	 * Wrong, if deleting characters.
	 */
	rptlines = tm->lno - fm->lno + 1;
	rptlabel = "deleted";

	return (0);
}
