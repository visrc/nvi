/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: delete.c,v 5.7 1992/06/07 16:47:58 bostic Exp $ (Berkeley) $Date: 1992/06/07 16:47:58 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>

#include "vi.h"
#include "exf.h"
#include "mark.h"
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
	char *bp, *p;

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
			EGETLINE(p, --tm->lno, len);
			tm->cno = len;
		}
		/* If deleting within a single line, it's easy. */
		if (tm->lno == fm->lno) {
			EGETLINE(p, fm->lno, len);
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
			EGETLINE(p, fm->lno, len);
			tlen = len;
			EGETLINE(p, tm->lno, len);
			tlen += len;		/* XXX Possible overflow! */
			if ((bp = malloc(tlen)) == NULL) {
				msg("Error: %s", strerror(errno));
				return (1);
			}

			/* Copy the start partial line into place. */
			EGETLINE(p, fm->lno, len);
			bcopy(p, bp, fm->cno);
			tlen = fm->cno;

			/* Copy the end partial line into place. */
			EGETLINE(p, tm->lno, len);
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
