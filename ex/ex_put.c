/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 5.13 1992/11/06 18:22:30 bostic Exp $ (Berkeley) $Date: 1992/11/06 18:22:30 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
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
