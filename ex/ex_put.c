/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 10.5 1995/09/21 12:07:24 bostic Exp $ (Berkeley) $Date: 1995/09/21 12:07:24 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../common/common.h"

/*
 * ex_put -- [line] pu[t] [buffer]
 *	Append a cut buffer into the file.
 *
 * PUBLIC: int ex_put __P((SCR *, EXCMD *));
 */
int
ex_put(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	MARK m;

	NEEDFILE(sp, cmdp);

	m.lno = sp->lno;
	m.cno = sp->cno;
	if (put(sp, NULL,
	    FL_ISSET(cmdp->iflags, E_C_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &m, 1))
		return (1);
	sp->lno = m.lno;
	sp->cno = m.cno;
	return (0);
}
