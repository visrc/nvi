/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 8.3 1993/08/25 16:44:29 bostic Exp $ (Berkeley) $Date: 1993/08/25 16:44:29 $";
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
	 * Historic vi had a special semantic.  If a buffer was cut in
	 * line mode, executing it had the same effect as if it had a
	 * trailing <newline> character.
	 */
	lmode = F_ISSET(cb, CB_LMODE);
	for (tp = cb->txthdr.next; tp != (TEXT *)&cb->txthdr; tp = tp->next)
		if ((lmode || tp->next != (TEXT *)&cb->txthdr) &&
		    term_push(sp, &sp->key, "\n", 1) ||
		    term_push(sp, &sp->key, tp->lb, tp->len))
			return (1);
	return (0);
}
