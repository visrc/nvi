/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 8.3 1993/08/26 12:03:23 bostic Exp $ (Berkeley) $Date: 1993/08/26 12:03:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <ctype.h>

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
	int cnt, displayed;
	char *p;

#define	NUMERICBUFS	"987654321"
	displayed = 0;
	for (cb = sp->cuts, cnt = 0; cnt < UCHAR_MAX; ++cb, ++cnt) {
		if (strchr(NUMERICBUFS, cnt))
			continue;
		if (cb->txthdr.next != NULL && cb->txthdr.next != &cb->txthdr) {
			displayed = 1;
			db(sp, charname(sp, cnt), cb);
		}
	}

	for (p = NUMERICBUFS; *p; ++p)
		if (sp->cuts[*p].txthdr.next != NULL &&
		    sp->cuts[*p].txthdr.next != &sp->cuts[*p].txthdr) {
			displayed = 1;
			db(sp, charname(sp, *p), &sp->cuts[*p]);
		}
	if (sp->cuts[DEFCB].txthdr.next != NULL &&
	    sp->cuts[DEFCB].txthdr.next != &sp->cuts[DEFCB].txthdr) {
		displayed = 1;
		db(sp, "default buffer", &sp->cuts[DEFCB]);
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
