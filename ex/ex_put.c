/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_put.c,v 9.1 1994/11/09 18:40:58 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:40:58 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_put -- [line] pu[t] [buffer]
 *
 *	Append a cut buffer into the file.
 */
int
ex_put(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	MARK m;

	NEEDFILE(sp, cmdp->cmd);

	m.lno = sp->lno;
	m.cno = sp->cno;
	if (put(sp, NULL, F_ISSET(cmdp, E_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &m, 1))
		return (1);
	sp->lno = m.lno;
	sp->cno = m.cno;
	return (0);
}
