/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_mark.c,v 8.8 1994/08/17 14:30:56 bostic Exp $ (Berkeley) $Date: 1994/08/17 14:30:56 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

int
ex_mark(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (cmdp->argv[0]->len != 1) {
		msgq(sp, M_ERR, "Mark names must be a single character");
		return (1);
	}
	return (mark_set(sp, ep, cmdp->argv[0]->bp[0], &cmdp->addr1, 1));
}
