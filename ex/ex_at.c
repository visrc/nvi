/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.20 1993/02/28 14:00:28 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:28 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_at -- :@[buffer]
 *	Execute the contents of the buffer.
 */
int
ex_at(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	static int lastbuf = OOBCB, recurse;
	static char rstack[UCHAR_MAX];
	CB *cb;
	TEXT *tp;
	int buffer, rval;

	if (cmdp->buffer == OOBCB) {
		if (lastbuf == OOBCB) {
			ep->msg(ep, M_ERROR, "No previous buffer to execute.");
			return (1);
		}
		buffer = lastbuf;
	} else
		buffer = cmdp->buffer;

	CBNAME(ep, buffer, cb);
	CBEMPTY(ep, buffer, cb);
		
	if (recurse == 0)
		memset(rstack, 0, sizeof(rstack));
	else if (rstack[buffer]) {
		ep->msg(ep, M_ERROR,
		    "Buffer %c already occurs in this command.", buffer);
		return (1);
	}

	rstack[buffer] = 1;
	++recurse;

	for (tp = cb->head;;) {
		if (rval = ex_cstring(ep, tp->lp, tp->len, 1))
			break;
		if (FF_ISSET(ep, F_FILE_RESET))
			break;
		if ((tp = tp->next) == NULL)
			break;
	}
		
	--recurse;
	return (0);
}
