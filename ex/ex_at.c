/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 8.6 1993/09/13 13:55:49 bostic Exp $ (Berkeley) $Date: 1993/09/13 13:55:49 $";
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
	CB *cb;
	TEXT *tp;
	int buffer, lmode;

	buffer = cmdp->buffer;
	if (buffer == cmdp->cmd->name[0]) {
		if (sp->at_lbuf == OOBCB) {
			msgq(sp, M_ERR, "No previous buffer to execute.");
			return (1);
		}
		buffer = sp->at_lbuf;
	}

	CBNAME(sp, buffer, cb);
	CBEMPTY(sp, buffer, cb);

	sp->at_lbuf = buffer;
		
	/*
	 * If the buffer was cut in line mode or had portions of more
	 * than one line, <newlines> are appended to each line as it
	 * is executed.
	 */
	tp = cb->txthdr.prev;
	lmode = F_ISSET(cb, CB_LMODE) || tp->prev != (TEXT *)&cb->txthdr;
	for (; tp != (TEXT *)&cb->txthdr; tp = tp->prev)
		if ((lmode || tp->prev != (TEXT *)&cb->txthdr) &&
		    term_push(sp, sp->key, "\n", 1) ||
		    term_push(sp, sp->key, tp->lb, tp->len))
			return (1);
	return (0);
}
