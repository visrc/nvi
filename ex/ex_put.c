/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 5.18 1993/02/24 12:55:45 bostic Exp $ (Berkeley) $Date: 1993/02/24 12:55:45 $";
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
ex_put(ep, cmdp)
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
		CBNAME(ep, buffer, cb);
	}
	
	m.lno = SCRLNO(ep);
	m.cno = SCRCNO(ep);
	if (put(ep, buffer, &cmdp->addr1, &m, 1))
		return (1);

	FF_SET(ep, F_AUTOPRINT);
	return (0);
}
