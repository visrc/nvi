/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 5.5 1993/03/25 14:59:43 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:43 $";
#endif /* not lint */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"

static void db __P((SCR *, char *, CB *));

/*
 * ex_bdisplay -- :bdisplay
 *	Display cut buffer contents.
 */
int
ex_bdisplay(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cb;
	int cnt;

	for (cb = cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt) {
		if (cb->head == NULL)
			continue;
		db(sp, CHARNAME(sp, cnt), cb);
	}
	if (cuts[DEFCB].head != NULL)
		db(sp, "default buffer", &cuts[DEFCB]);
	return (0);
}

static void
db(sp, name, cb)
	SCR *sp;
	char *name;
	CB *cb;
{
	TEXT *tp;

	(void)fprintf(sp->stdfp, "%s%s\n", name,
	    cb->flags & CB_LMODE ? ": line mode" : "");
	for (tp = cb->head; tp; tp = tp->next)
		(void)fprintf(sp->stdfp, "%.*s\n", tp->len, tp->lp);
}
