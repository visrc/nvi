/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 8.6 1994/01/09 23:24:02 bostic Exp $ (Berkeley) $Date: 1994/01/09 23:24:02 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "excmd.h"

enum which {COPY, MOVE};
static int cm __P((SCR *, EXF *, EXCMDARG *, enum which));

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
	return (cm(sp, ep, cmdp, COPY));
}

/*
 * ex_move -- :[line [,line]] co[py] line
 *	Move selected lines.
 */
int
ex_move(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (cm(sp, ep, cmdp, MOVE));
}

static int
cm(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	CB cb;
	MARK fm1, fm2, m, tm;
	recno_t diff;
	int rval;

	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	tm.lno = cmdp->lineno;
	tm.cno = 0;

	/* Make sure the destination is valid. */
	if (cmd == MOVE && tm.lno >= fm1.lno && tm.lno < fm2.lno) {
		msgq(sp, M_ERR, "Destination line is inside move range.");
		return (1);
	}

	/* Save the text to a cut buffer. */
	memset(&cb, 0, sizeof(cb));
	CIRCLEQ_INIT(&cb.textq);
	if (cut(sp, ep, &cb, NULL, &fm1, &fm2, CUT_LINEMODE))
		return (1);

	/* If we're not copying, delete the old text and adjust tm. */
	if (cmd == MOVE) {
		if (delete(sp, ep, &fm1, &fm2, 1)) {
			rval = 1;
			goto err;
		}
		if (tm.lno >= fm1.lno)
			tm.lno -= (fm2.lno - fm1.lno) + 1;
	}

	/* Add the new text. */
	if (put(sp, ep, &cb, NULL, &tm, &m, 1)) {
		rval = 1;
		goto err;
	}

	/*
	 * Move and copy put the cursor on the last line moved or copied.
	 * The returned cursor from the put routine is the first line put,
	 * not the last, because that's the semantics of vi.
	 */
	diff = (fm2.lno - fm1.lno) + 1;
	sp->lno = m.lno + (diff - 1);
	sp->cno = 0;

	sp->rptlines[cmd == COPY ? L_COPIED : L_MOVED] += diff;
	rval = 0;

err:	(void)text_lfree(&cb.textq);
	return (rval);
}
