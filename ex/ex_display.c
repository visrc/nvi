/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 5.4 1993/02/16 20:10:03 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:03 $";
#endif /* not lint */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

static void db __P((EXF *, char *, CB *));

/*
 * ex_bdisplay -- :bdisplay
 *	Display cut buffer contents.
 */
int
ex_bdisplay(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cb;
	int cnt;

	for (cb = cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt) {
		if (cb->head == NULL)
			continue;
		db(ep, CHARNAME(cnt), cb);
	}
	if (cuts[DEFCB].head != NULL)
		db(ep, "default buffer", &cuts[DEFCB]);
	return (0);
}

static void
db(ep, name, cb)
	EXF *ep;
	char *name;
	CB *cb;
{
	TEXT *tp;

	(void)fprintf(ep->stdfp, "%s%s\n", name,
	    cb->flags & CB_LMODE ? ": line mode" : "");
	for (tp = cb->head; tp; tp = tp->next)
		(void)fprintf(ep->stdfp, "%.*s\n", tp->len, tp->lp);
}
