/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 8.8 1993/11/04 16:16:51 bostic Exp $ (Berkeley) $Date: 1993/11/04 16:16:51 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_at -- :@[@ | buffer]
 *	    :*[* | buffer]
 *
 *	Execute the contents of the buffer.
 */
int
ex_at(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB *cbp;
	TEXT *tp;
	int name, lmode;

	name = cmdp->buffer;

	/* Historically, @@ and ** execute the last buffer. */
	if (name == cmdp->cmd->name[0]) {
		if (!sp->at_lbuf_set) {
			msgq(sp, M_ERR, "No previous buffer to execute.");
			return (1);
		}
		name = sp->at_lbuf;
	}

	CBEMPTY(sp, cbp, name);

	/* Save for reuse. */
	sp->at_lbuf = name;
	sp->at_lbuf_set = 1;
		
	/*
	 * If the buffer was cut in line mode or had portions of more
	 * than one line, <newlines> are appended to each line as it
	 * is executed.
	 */
	tp = cbp->txthdr.prev;
	lmode = F_ISSET(cbp, CB_LMODE) || tp->prev != (TEXT *)&cbp->txthdr;
	for (; tp != (TEXT *)&cbp->txthdr; tp = tp->prev)
		if ((lmode || tp->prev != (TEXT *)&cbp->txthdr) &&
		    term_push(sp, sp->gp->key, "\n", 1) ||
		    term_push(sp, sp->gp->key, tp->lb, tp->len))
			return (1);
	return (0);
}
