/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 8.12 1993/12/02 10:55:03 bostic Exp $ (Berkeley) $Date: 1993/12/02 10:55:03 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "tag.h"
#include "excmd.h"

static int	bdisplay __P((SCR *, EXF *));
static void	db __P((SCR *, CB *));

/*
 * ex_display -- :display b[uffers] | s[creens] | t[ags]
 *
 *	Display buffers, tags or screens.
 */
int
ex_display(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	switch (cmdp->argv[0]->bp[0]) {
	case 'b':
		return (bdisplay(sp, ep));
	case 's':
		return (ex_sdisplay(sp, ep));
	case 't':
		return (ex_tagdisplay(sp, ep));
	}
	msgq(sp, M_ERR,
	    "Unknown display argument %s, use b[uffers], s[creens], or t[ags].",
	    cmdp->argv[0]);
	return (1);
}

/*
 * bdisplay --
 *
 *	Display buffers.
 */
static int
bdisplay(sp, ep)
	SCR *sp;
	EXF *ep;
{
	CB *cbp;

	if (sp->gp->cutq.lh_first == NULL) {
		(void)ex_printf(EXCOOKIE, "No cut buffers to display.");
		return (0);
	}

	/* Display regular cut buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq)
			db(sp, cbp);
	}
	/* Display numbered buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (!isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq)
			db(sp, cbp);
	}
	return (0);
}

/*
 * db --
 *	Display a buffer.
 */
static void
db(sp, cbp)
	SCR *sp;
	CB *cbp;
{
	TEXT *tp;
	size_t len;
	char *p;

	(void)ex_printf(EXCOOKIE, "********** %s%s\n", charname(sp, cbp->name),
	    F_ISSET(cbp, CB_LMODE) ? " (line mode)" : "");
	for (tp = cbp->textq.cqh_first;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_next) {
		for (len = tp->len, p = tp->lb; len--;)
			(void)ex_printf(EXCOOKIE, "%s", charname(sp, *p++));
		(void)ex_printf(EXCOOKIE, "\n");
	}
}
