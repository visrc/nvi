/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_move.c,v 5.2 1992/04/04 10:02:40 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {COPY, MOVE};
static void copy __P((CMDARG *, enum which));

void
ex_copy(cmdp)
	CMDARG *cmdp;
{
	move(cmdp, COPY);
}

void
ex_move(cmdp)
	CMDARG *cmdp;
{
	move(cmdp, MOVE);
}

/* move or copy selected lines */
static void
copy(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	MARK	frommark;
	MARK	tomark;
	MARK	destmark;
	char *extra;

	frommark = cmdp->addr1;
	tomark = cmdp->addr2;
	extra = cmdp->argv[0];


	/* parse the destination linespec.  No defaults.  Line 0 is okay */
	destmark = cursor;
	if (!strcmp(extra, "0"))
	{
		destmark = 0L;
	}
	else {
		CMDARG __xxx;
		if (linespec(extra, &__xxx) == extra || !destmark) {
			msg("invalid destination address");
			return;
		}
		destmark = __xxx.addr1;
	}

	/* flesh the marks out to encompass whole lines */
	frommark &= ~(BLKSIZE - 1);
	tomark = (tomark & ~(BLKSIZE - 1)) + BLKSIZE;
	destmark = (destmark & ~(BLKSIZE - 1)) + BLKSIZE;

	/* make sure the destination is valid */
	if (cmd == MOVE && destmark >= frommark && destmark < tomark)
	{
		msg("invalid destination address");
	}

	/* Do it */
	ChangeText
	{
		/* save the text to a cut buffer */
		cutname('\0');
		cut(frommark, tomark);

		/* if we're not copying, delete the old text & adjust destmark */
		if (cmd == MOVE)
		{
			delete(frommark, tomark);
			if (destmark >= frommark)
			{
				destmark -= (tomark - frommark);
			}
		}

		/* add the new text */
		paste(destmark, FALSE, FALSE);
	}

	/* move the cursor to the last line of the moved text */
	cursor = destmark + (tomark - frommark) - BLKSIZE;
	if (cursor < MARK_FIRST || cursor >= MARK_LAST + BLKSIZE)
	{
		cursor = MARK_LAST;
	}

	/* Reporting... */
	rptlabel = (cmd == MOVE ? "moved" : "copied");
}

