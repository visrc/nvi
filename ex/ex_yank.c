/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_yank.c,v 8.3 1994/01/09 14:20:57 bostic Exp $ (Berkeley) $Date: 1994/01/09 14:20:57 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_yank -- :[line [,line]] ya[nk] [buffer] [count]
 *
 *	Yank the lines into a buffer.
 */
int
ex_yank(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (cut(sp, ep, NULL,
	    F_ISSET(cmdp, E_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE));
}
