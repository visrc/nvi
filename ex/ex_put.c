/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 8.2 1993/11/04 16:16:54 bostic Exp $ (Berkeley) $Date: 1993/11/04 16:16:54 $";
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
	if (put(sp, ep,
	    F_ISSET(cmdp, E_BUFFER) ? DEFCB : cmdp->buffer,
	    &cmdp->addr1, &m, 1))
		return (1);

	F_SET(sp, S_AUTOPRINT);
	return (0);
}
