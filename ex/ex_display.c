/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 5.2 1992/11/07 10:23:42 bostic Exp $ (Berkeley) $Date: 1992/11/07 10:23:42 $";
#endif /* not lint */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

static void db __P((char *, CB *));

/*
 * ex_bdisplay -- :bdisplay
 *	Display cut buffer contents.
 */
int
ex_bdisplay(cmdp)
	EXCMDARG *cmdp;
{
	CB *cb;
	int cnt;

	for (cb = cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt) {
		if (cb->head == NULL)
			continue;
		db(charname(cnt), cb);
	}
	if (cuts[DEFCB].head != NULL)
		db("default buffer", &cuts[DEFCB]);
	return (0);
}

static void
db(name, cb)
	char *name;
	CB *cb;
{
	TEXT *tp;

	(void)fprintf(curf->stdfp, "%s%s\n", name,
	    cb->flags & CB_LMODE ? ": line mode" : "");
	for (tp = cb->head; tp; tp = tp->next)
		(void)fprintf(curf->stdfp, "%.*s\n", tp->len, tp->lp);
}
