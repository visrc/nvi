/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 5.19 1993/03/25 15:00:00 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:00:00 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "screen.h"

int
ex_put(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	MARK m;
	CB *cb;
	int buffer;

	if (cmdp->buffer == OOBCB)
		buffer = DEFCB;
	else {
		buffer = cmdp->buffer;
		CBNAME(sp, buffer, cb);
	}
	
	m.lno = ep->lno;
	m.cno = ep->cno;
	if (put(sp, ep, buffer, &cmdp->addr1, &m, 1))
		return (1);

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
