/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 5.17 1992/12/05 11:08:44 bostic Exp $ (Berkeley) $Date: 1992/12/05 11:08:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

enum which {COPY, MOVE};
static int cm __P((EXCMDARG *, enum which));

/*
 * ex_copy -- :[line [,line]] co[py] line [flags]
 *	Copy selected lines.
 */
int
ex_copy(cmdp)
	EXCMDARG *cmdp;
{
	return (cm(cmdp, COPY));
}

/*
 * ex_move -- :[line [,line]] co[py] line
 *	Move selected lines.
 */
int
ex_move(cmdp)
	EXCMDARG *cmdp;
{
	return (cm(cmdp, MOVE));
}

static int
cm(cmdp, cmd)
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
		msg("Destination line is inside move range.");
		return (1);
	}

	/* Save the text to a cut buffer. */
	cut(curf, DEFCB, &fm1, &fm2, 1);

	/* If we're not copying, delete the old text & adjust tm. */
	if (cmd == MOVE) {
		delete(curf, &fm1, &fm2, 1);
		if (tm.lno >= fm1.lno)
			tm.lno -= fm2.lno - fm1.lno;
	}

	/* Add the new text. */
	m.lno = curf->lno;
	m.cno = curf->cno;
	(void)put(curf, DEFCB, &tm, &m, 1);

	if (curf->lno < 1)
		curf->lno = 1;
	else {
		lline = file_lline(curf);
		if (curf->lno > lline)
			curf->lno = lline;
	}

	/* Reporting. */
	if ((curf->rptlines = fm2.lno - fm1.lno) == 0)
		curf->rptlines = 1;
	curf->rptlabel = (cmd == MOVE ? "moved" : "copied");

	autoprint = 1;
	return (0);
}
