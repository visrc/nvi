/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 8.7 1993/11/04 16:16:53 bostic Exp $ (Berkeley) $Date: 1993/11/04 16:16:53 $";
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
	GS *gp;
	int cnt, displayed;
	char *p;

	displayed = 0;
	for (cbp = sp->gp->cutq.le_next; cbp != NULL; cbp = cbp->q.qe_next) {
		if (isdigit(cbp->name))
			continue;
		if (cbp->txthdr.next != NULL &&
		    cbp->txthdr.next != &cbp->txthdr) {
			displayed = 1;
			db(sp, cbp);
		}
	}
	for (cbp = sp->gp->cutq.le_next; cbp != NULL; cbp = cbp->q.qe_next) {
		if (!isdigit(cbp->name))
			continue;
		if (cbp->txthdr.next != NULL &&
		    cbp->txthdr.next != &cbp->txthdr) {
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
	for (tp = cbp->txthdr.next; tp != (TEXT *)&cbp->txthdr; tp = tp->next) {
		for (len = tp->len, p = tp->lb; len--;)
			(void)ex_printf(EXCOOKIE, "%s", charname(sp, *p++));
		(void)ex_printf(EXCOOKIE, "\n");
	}
}
