/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 8.5 1993/09/30 12:02:41 bostic Exp $ (Berkeley) $Date: 1993/09/30 12:02:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

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
	GS *gp;
	int cnt, displayed;
	char *p;

#define	NUMERICBUFS	"987654321"
	displayed = 0;
	for (cb = (gp = sp->gp)->cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt) {
		if (strchr(NUMERICBUFS, cnt))
			continue;
		if (cb->txthdr.next != NULL && cb->txthdr.next != &cb->txthdr) {
			displayed = 1;
			db(sp, charname(sp, cnt), cb);
		}
	}

	for (p = NUMERICBUFS; *p; ++p)
		if (gp->cuts[*p].txthdr.next != NULL &&
		    gp->cuts[*p].txthdr.next != &gp->cuts[*p].txthdr) {
			displayed = 1;
			db(sp, charname(sp, *p), &gp->cuts[*p]);
		}
	if (gp->cuts[DEFCB].txthdr.next != NULL &&
	    gp->cuts[DEFCB].txthdr.next != &gp->cuts[DEFCB].txthdr) {
		displayed = 1;
		db(sp, "default buffer", &gp->cuts[DEFCB]);
	}
	if (!displayed)
		msgq(sp, M_VINFO, "No buffers to display.");
	return (0);
}

static void
db(sp, name, cb)
	SCR *sp;
	char *name;
	CB *cb;
{
	TEXT *tp;
	size_t len;
	char *p;

	(void)fprintf(sp->stdfp, "================ %s%s\n", name,
	    F_ISSET(cb, CB_LMODE) ? " (line mode)" : "");
	for (tp = cb->txthdr.next; tp != (TEXT *)&cb->txthdr; tp = tp->next) {
		for (len = tp->len, p = tp->lb; len--;)
			(void)fprintf(sp->stdfp, "%s", charname(sp, *p++));
		(void)putc('\n', sp->stdfp);
	}
}
