/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 5.23 1993/05/02 13:38:18 bostic Exp $ (Berkeley) $Date: 1993/05/02 13:38:18 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

int
ex_put(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	MARK m;

	m.lno = sp->lno;
	m.cno = sp->cno;
	if (put(sp, ep,
	    cmdp->buffer == OOBCB ? DEFCB : cmdp->buffer, &cmdp->addr1, &m, 1))
		return (1);

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
