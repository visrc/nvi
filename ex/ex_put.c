/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 8.4 1994/01/09 14:20:56 bostic Exp $ (Berkeley) $Date: 1994/01/09 14:20:56 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_put -- [line] pu[t] [buffer]
 *
 *	Append a cut buffer into the file.
 */
int
ex_put(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	MARK m;

	m.lno = sp->lno;
	m.cno = sp->cno;
	if (put(sp, ep, NULL, F_ISSET(cmdp, E_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &m, 1))
		return (1);
	return (0);
}
