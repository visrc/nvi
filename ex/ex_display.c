/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_display.c,v 8.24 1994/08/31 17:17:06 bostic Exp $ (Berkeley) $Date: 1994/08/31 17:17:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "tag.h"
#include "excmd.h"

static int	bdisplay __P((SCR *, EXF *));
static void	db __P((SCR *, CB *, CHAR_T *));

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
#undef	ARG
#define	ARG	"buffers"
		if (cmdp->argv[0]->len >= sizeof(ARG) ||
		    memcmp(cmdp->argv[0]->bp, ARG, cmdp->argv[0]->len))
			break;
		return (bdisplay(sp, ep));
	case 's':
#undef	ARG
#define	ARG	"screens"
		if (cmdp->argv[0]->len >= sizeof(ARG) ||
		    memcmp(cmdp->argv[0]->bp, ARG, cmdp->argv[0]->len))
			break;
		return (ex_sdisplay(sp, ep));
	case 't':
#undef	ARG
#define	ARG	"tags"
		if (cmdp->argv[0]->len >= sizeof(ARG) ||
		    memcmp(cmdp->argv[0]->bp, ARG, cmdp->argv[0]->len))
			break;
		return (ex_tagdisplay(sp, ep));
	}
	ex_message(sp, cmdp, EXM_USAGE);
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

	if (sp->gp->cutq.lh_first == NULL && sp->gp->dcbp == NULL) {
		(void)ex_printf(EXCOOKIE, "No cut buffers to display.\n");
		return (0);
	}

	/* Buffers can be infinitely long, make it interruptible. */
	F_SET(sp, S_INTERRUPTIBLE);

	/* Display regular cut buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq)
			db(sp, cbp, NULL);
		if (INTERRUPTED(sp))
			return (0);
	}
	/* Display numbered buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (!isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq)
			db(sp, cbp, NULL);
		if (INTERRUPTED(sp))
			return (0);
	}
	/* Display default buffer. */
	if ((cbp = sp->gp->dcbp) != NULL)
		db(sp, cbp, "default buffer");
	return (0);
}

/*
 * db --
 *	Display a buffer.
 */
static void
db(sp, cbp, name)
	SCR *sp;
	CB *cbp;
	CHAR_T *name;
{
	CHAR_T *p;
	TEXT *tp;
	size_t len;

	(void)ex_printf(EXCOOKIE, "********** %s%s\n",
	    name == NULL ? KEY_NAME(sp, cbp->name) : name,
	    F_ISSET(cbp, CB_LMODE) ? " (line mode)" : " (character mode)");
	for (tp = cbp->textq.cqh_first;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_next) {
		for (len = tp->len, p = tp->lb; len--; ++p) {
			(void)ex_printf(EXCOOKIE, "%s", KEY_NAME(sp, *p));
			if (INTERRUPTED(sp))
				return;
		}
		(void)ex_printf(EXCOOKIE, "\n");
	}
}
