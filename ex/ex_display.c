/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 8.10 1993/11/18 10:08:55 bostic Exp $ (Berkeley) $Date: 1993/11/18 10:08:55 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

static void db __P((SCR *, CB *));

/*
 * ex_bdisplay -- :bdisplay
 *
 *	Display cut buffer contents.
 */
int
ex_bdisplay(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cbp;
	int displayed;

	displayed = 0;
	/* Display regular cut buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq) {
			displayed = 1;
			db(sp, cbp);
		}
	}
	/* Display numbered buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (!isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq) {
			displayed = 1;
			db(sp, cbp);
		}
	}
	if (!displayed)
		msgq(sp, M_VINFO, "No buffers to display.");
	return (0);
}

static void
db(sp, cbp)
	SCR *sp;
	CB *cbp;
{
	TEXT *tp;
	size_t len;
	char *p;

	(void)ex_printf(EXCOOKIE,
	    "================ %s%s\n", charname(sp, cbp->name),
	    F_ISSET(cbp, CB_LMODE) ? " (line mode)" : "");
	for (tp = cbp->textq.cqh_first;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_next) {
		for (len = tp->len, p = tp->lb; len--;)
			(void)ex_printf(EXCOOKIE, "%s", charname(sp, *p++));
		(void)ex_printf(EXCOOKIE, "\n");
	}
}
