/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.11 1992/05/21 12:53:56 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:53:56 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "excmd.h"
#include "cut.h"
#include "extern.h"

/*
 * ex_at -- :@[buffer]
 *	Execute the contents of the buffer.
 */
int
ex_at(cmdp)
	EXCMDARG *cmdp;
{
	static int lastbuf = OOBCB, recurse;
	static char rstack[UCHAR_MAX];
	CB *cb;
	CBLINE *lp;
	int buffer, rval;

	if (cmdp->buffer == OOBCB) {
		if (lastbuf == OOBCB) {
			msg("No previous buffer to execute.");
			return (1);
		}
		buffer = lastbuf;
	} else
		buffer = cmdp->buffer;

	CBNAME(buffer, cb);
	CBEMPTY(buffer, cb);
		
	if (recurse == 0)
		bzero(rstack, sizeof(rstack));
	else if (rstack[buffer]) {
		msg("Buffer %c already occurs in this command.", buffer);
		return (1);
	}

	rstack[buffer] = 1;
	++recurse;

	for (lp = cb->head;;) {
		if (rval = ex_cstring(lp->lp, lp->len, 1))
			break;
		if ((lp = lp->next) == NULL)
			break;
	}
		
	--recurse;
	return (0);
}
