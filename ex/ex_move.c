/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 5.9 1992/05/15 11:08:13 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:08:13 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
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
	MARK fm1, fm2, tm;
	char *extra;

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
	cutname('\0');
	cut(&fm1, &fm2);

	/* If we're not copying, delete the old text & adjust tm. */
	if (cmd == MOVE) {
		delete(&fm1, &fm2);
		if (tm.lno >= fm1.lno)
			tm.lno -= fm2.lno - fm1.lno;
	}

	/* Add the new text. */
	paste(&tm, 0, 0);

	/* Move the cursor to the last line of the moved text. */
	cursor.lno = tm.lno + (fm2.lno - fm1.lno);
	nlines = file_lline(curf);
	if (cursor.lno < 1)
		cursor.lno = 1;
	else if (cursor.lno > nlines)
		cursor.lno = nlines;

	/* Reporting. */
	if ((rptlines = fm2.lno - fm1.lno) == 0)
		rptlines = 1;
	rptlabel = (cmd == MOVE ? "moved" : "copied");

	autoprint = 1;
	return (0);
}
