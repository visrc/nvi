/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_yank.c,v 10.5 1995/09/21 12:07:47 bostic Exp $ (Berkeley) $Date: 1995/09/21 12:07:47 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_yank -- :[line [,line]] ya[nk] [buffer] [count]
 *	Yank the lines into a buffer.
 *
 * PUBLIC: int ex_yank __P((SCR *, EXCMD *));
 */
int
ex_yank(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	NEEDFILE(sp, cmdp);

	/*
	 * !!!
	 * Historically, yanking lines in ex didn't count toward the
	 * number-of-lines-yanked report.
	 */
	return (cut(sp,
	    FL_ISSET(cmdp->iflags, E_C_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE));
}
