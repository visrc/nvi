/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_delete.c,v 5.2 1992/04/04 10:02:31 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:31 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {DELETE, YANK};
static void Xdelete __P((CMDARG *, enum which));

void
ex_delete(cmdp)
	CMDARG *cmdp;
{
	Xdelete(cmdp, DELETE);
}

void
ex_yank(cmdp)
	CMDARG *cmdp;
{
	Xdelete(cmdp, YANK);
}

static void
Xdelete(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	MARK	curs2;	/* an altered form of the cursor */
	MARK	frommark;
	MARK	tomark;
	char *extra;

	frommark = cmdp->addr1;
	tomark = cmdp->addr2;
	extra = cmdp->argv[0];

	/* choose your cut buffer */
	if (*extra == '"')
	{
		extra++;
	}
	if (*extra)
	{
		cutname(*extra);
	}

	/* make sure we're talking about whole lines here */
	frommark = frommark & ~(BLKSIZE - 1);
	tomark = (tomark & ~(BLKSIZE - 1)) + BLKSIZE;

	/* yank the lines */
	cut(frommark, tomark);

	if (cmd == DELETE) {
		curs2 = cursor;
		ChangeText
		{
			/* delete the lines */
			delete(frommark, tomark);
		}
		if (curs2 > tomark)
		{
			cursor = curs2 - tomark + frommark;
		}
		else if (curs2 > frommark)
		{
			cursor = frommark;
		}
	}
}
