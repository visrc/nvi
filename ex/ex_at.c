/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.24 1993/04/13 16:22:39 bostic Exp $ (Berkeley) $Date: 1993/04/13 16:22:39 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_at -- :@[buffer]
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
	int buffer, rval;

	if (cmdp->buffer == OOBCB) {
		if (sp->exat_lbuf == OOBCB) {
			msgq(sp, M_ERR, "No previous buffer to execute.");
			return (1);
		}
		buffer = sp->exat_lbuf;
	} else
		buffer = cmdp->buffer;

	CBNAME(sp, buffer, cb);
	CBEMPTY(sp, buffer, cb);
		
	if (sp->exat_recurse == 0)
		memset(sp->exat_stack, 0, sizeof(sp->exat_stack));
	else if (sp->exat_stack[buffer]) {
		msgq(sp, M_ERR,
		    "Buffer %s already occurs in this command.",
		    charname(sp, buffer));
		return (1);
	}

	sp->exat_stack[buffer] = 1;
	++sp->exat_recurse;

	for (tp = cb->txthdr.next; tp != (TEXT *)&cb->txthdr; tp = tp->next) {
		if (rval = ex_cstring(sp, ep, tp->lb, tp->len))
			break;
		if (F_ISSET(sp, S_FILE_CHANGED))
			break;
	}
		
	--sp->exat_recurse;
	return (0);
}
