/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 8.7 1994/03/06 10:14:02 bostic Exp $ (Berkeley) $Date: 1994/03/06 10:14:02 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_copy -- :[line [,line]] co[py] line [flags]
 *	Copy selected lines.
 */
int
ex_copy(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB cb;
	MARK fm1, fm2, m, tm;
	recno_t cnt;
	int rval;

	/*
	 * It's possible to copy things into the area that's being
	 * copied, e.g. "2,5copy3" is legitimate.  Save the text to
	 * a cut buffer.
	 */
	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	memset(&cb, 0, sizeof(cb));
	CIRCLEQ_INIT(&cb.textq);
	if (cut(sp, ep, &cb, NULL, &fm1, &fm2, CUT_LINEMODE))
		return (1);

	/* Put the text into place. */
	tm.lno = cmdp->lineno;
	tm.cno = 0;
	if (put(sp, ep, &cb, NULL, &tm, &m, 1))
		rval = 1;
	else {
		/*
		 * Copy puts the cursor on the last line copied.  The cursor
		 * returned by the put routine is the first line put, not the
		 * last, because that's the historic semantic of vi.
		 */
		cnt = (fm2.lno - fm1.lno) + 1;
		sp->lno = m.lno + (cnt - 1);
		sp->cno = 0;

		sp->rptlines[L_COPIED] += cnt;
		rval = 0;
	}
	text_lfree(&cb.textq);
	return (rval);
}

/*
 * ex_move -- :[line [,line]] mo[ve] line
 *	Move selected lines.
 */
int
ex_move(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	LMARK *lmp;
	MARK fm1, fm2, m;
	recno_t cnt, fl, tl, mfl, mtl;
	size_t len;
	int mark_reset;
	char *p;

	/*
	 * It's not possible to move things into the area that's being
	 * moved.
	 */
	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	if (cmdp->lineno >= fm1.lno && cmdp->lineno < fm2.lno) {
		msgq(sp, M_ERR, "Destination line is inside move range.");
		return (1);
	}

	/*
	 * Log the positions of any marks in the to-be-deleted lines.  This
	 * has to work with the logging code.  What happens is that we log
	 * the old mark positions, make the changes, then log the new mark
	 * positions.  Then the marks end up in the right positions no matter
	 * which way the log is traversed.
	 *
	 * XXX
	 * Reset the MARK_USERSET flag so that the log can undo the mark.
	 * This isn't very clean, and should probably be fixed.
	 */
	fl = fm1.lno;
	tl = cmdp->lineno;
	mark_reset = 0;
	/* Log the old positions of the marks. */
	for (lmp = ep->marks.lh_first; lmp != NULL; lmp = lmp->q.le_next)
		if (lmp->name != ABSMARK1 &&
		    lmp->lno >= fl && lmp->lno <= tl) {
			mark_reset = 1;
			F_CLR(lmp, MARK_USERSET);
			(void)log_mark(sp, ep, lmp);
		}

	/* Move the lines. */
	cnt = (fm2.lno - fm1.lno) + 1;
	if (tl > fl) {				/* Destination > source. */
		mfl = tl - cnt;
		mtl = tl;
		while (cnt--) {
			if ((p = file_gline(sp, ep, fl, &len)) == NULL)
				return (1);
			if (file_aline(sp, ep, 1, tl, p, len))
				return (1);
			if (mark_reset)
				for (lmp = ep->marks.lh_first;
				    lmp != NULL; lmp = lmp->q.le_next)
					if (lmp->name != ABSMARK1 &&
					    lmp->lno == fl)
						lmp->lno = tl + 1;
			if (file_dline(sp, ep, fl))
				return (1);
		}
	} else {				/* Destination < source. */
		mfl = tl;
		mtl = tl + cnt;
		while (cnt--) {
			if ((p = file_gline(sp, ep, fl, &len)) == NULL)
				return (1);
			if (file_aline(sp, ep, 1, tl++, p, len))
				return (1);
			if (mark_reset)
				for (lmp = ep->marks.lh_first;
				    lmp != NULL; lmp = lmp->q.le_next)
					if (lmp->name != ABSMARK1 &&
					    lmp->lno == fl)
						lmp->lno = tl;
			++fl;
			if (file_dline(sp, ep, fl))
				return (1);
		}
	}

	/* Log the new positions of the marks. */
	if (mark_reset)
		for (lmp = ep->marks.lh_first;
		    lmp != NULL; lmp = lmp->q.le_next)
			if (lmp->name != ABSMARK1 &&
			    lmp->lno >= mfl && lmp->lno <= mtl)
				(void)log_mark(sp, ep, lmp);

	/* Move puts the cursor on the last line moved. */
	cnt = (fm2.lno - fm1.lno) + 1;
	sp->lno = fm1.lno + cnt;
	sp->cno = 0;

	sp->rptlines[L_MOVED] += cnt;
	return (0);
}
