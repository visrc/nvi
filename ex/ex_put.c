/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 5.9 1992/06/07 13:47:06 bostic Exp $ (Berkeley) $Date: 1992/06/07 13:47:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "cut.h"
#include "extern.h"

int
ex_put(cmdp)
	EXCMDARG *cmdp;
{
	MARK m;
	CB *cb;
	int buffer;

	if (cmdp->buffer == OOBCB)
		buffer = DEFCB;
	else {
		buffer = cmdp->buffer;
		CBNAME(buffer, cb);
	}
	
	m.lno = curf->lno;
	m.cno = curf->cno;
	if (put(buffer, &cmdp->addr1, &m, 1))
		return (1);

	autoprint = 1;
	return (0);
}
