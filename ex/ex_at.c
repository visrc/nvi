/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.22 1993/03/26 13:38:43 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:38:43 $";
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
			msgq(sp, M_ERROR, "No previous buffer to execute.");
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
		msgq(sp, M_ERROR,
		    "Buffer %s already occurs in this command.",
		    charname(sp, buffer));
		return (1);
	}

	sp->exat_stack[buffer] = 1;
	++sp->exat_recurse;

	for (tp = cb->head;;) {
		if (rval = ex_cstring(sp, ep, tp->lp, tp->len, 1))
			break;
		if (F_ISSET(sp, S_FILE_CHANGED))
			break;
		if ((tp = tp->next) == NULL)
			break;
	}
		
	--sp->exat_recurse;
	return (0);
}
