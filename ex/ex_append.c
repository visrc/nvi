/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 5.2 1992/04/04 10:02:26 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:26 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {APPEND, CHANGE};
static void Xchange __P((CMDARG *, enum which));

/*
 * ex_append (:address append)
 *	Append one ore more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
void
ex_append(cmdp)
	CMDARG *cmdp;
{
	Xchange(cmdp, APPEND);
}

void
ex_change(cmdp)
	CMDARG *cmdp;
{
	Xchange(cmdp, CHANGE);
}

static void
Xchange(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	long l;			/* Line counter. */

	m = cmdp->addrcnt ? cmdp->addr1 : cursor;

	ChangeText
	{
		/* If we're doing a change, delete the old version. */
		if (cmd == CHANGE)
			ex_delete(cmdp);

		/* New lines start at the frommark line, or after it. */
		l = markline(m);
		if (cmd != APPEND)
 			l++;

		/* Get lines until no more lines, or "." line, and insert. */
		while (vgets('\0', tmpblk.c, BLKSIZE) >= 0) {
			addch('\n');
			if (!strcmp(tmpblk.c, "."))
				break;

			(void)strcat(tmpblk.c, "\n");
			add(MARK_AT_LINE(l), tmpblk.c);
			l++;
		}
	}

	/* On the odd chance that we're calling this from vi mode. */
	redraw(MARK_UNSET, FALSE);
}
