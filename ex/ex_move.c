/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 8.1 1993/06/09 22:24:47 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:24:47 $";
#endif /* not lint */

#include <sys/types.h>

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
	MARK fm1, fm2, m, tm;
	recno_t lline;

	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	tm.lno = cmdp->lineno;
	tm.cno = 0;

	/* Make sure the destination is valid. */
	if (cmd == MOVE && tm.lno >= fm1.lno && tm.lno < fm2.lno) {
		msgq(sp, M_ERR,
		    "Destination line is inside move range.");
		return (1);
	}

	/* Save the text to a cut buffer. */
	if (cut(sp, ep, DEFCB, &fm1, &fm2, 1))
		return (1);

	/* If we're not copying, delete the old text & adjust tm. */
	if (cmd == MOVE) {
		if (delete(sp, ep, &fm1, &fm2, 1))
			return (1);
		if (tm.lno >= fm1.lno)
			tm.lno -= fm2.lno - fm1.lno;
	}

	/* Add the new text. */
	m.lno = sp->lno;
	m.cno = sp->cno;
	if (put(sp, ep, DEFCB, &tm, &m, 1))
		return (1);

	if (sp->lno < 1)
		sp->lno = 1;
	else {
		if (file_lline(sp, ep, &lline))
			return (1);
		if (sp->lno > lline)
			sp->lno = lline;
	}

	/* Reporting. */
	sp->rptlines[cmd == COPY ? L_COPIED : L_MOVED] += fm2.lno - fm1.lno;

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
