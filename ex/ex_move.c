/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 5.11 1992/06/07 13:46:46 bostic Exp $ (Berkeley) $Date: 1992/06/07 13:46:46 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "cut.h"
#include "extern.h"

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

	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	tm.lno = cmdp->lineno;
	tm.cno = 0;

	/* Make sure the destination is valid. */
	if (cmd == MOVE && tm.lno >= fm1.lno && tm.lno < fm2.lno) {
		msg("Target line is inside move range.");
		return (1);
	}

	/* Save the text to a cut buffer. */
	cut(DEFCB, &fm1, &fm2, 1);

	/* If we're not copying, delete the old text & adjust tm. */
	if (cmd == MOVE) {
		delete(&fm1, &fm2);
		if (tm.lno >= fm1.lno)
			tm.lno -= fm2.lno - fm1.lno;
	}

	/* Add the new text. */
	m.lno = curf->lno;
	m.cno = curf->cno;
	(void)put(DEFCB, &tm, &m, 1);

	nlines = file_lline(curf);
	if (curf->lno < 1)
		curf->lno = 1;
	else if (curf->lno > nlines)
		curf->lno = nlines;

	/* Reporting. */
	if ((rptlines = fm2.lno - fm1.lno) == 0)
		rptlines = 1;
	rptlabel = (cmd == MOVE ? "moved" : "copied");

	autoprint = 1;
	return (0);
}
