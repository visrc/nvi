/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_set.c,v 8.3 1994/03/08 19:39:40 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:39:40 $";
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
ex_set(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		opts_dump(sp, CHANGED_DISPLAY);
		break;
	default:
		opts_set(sp, cmdp->argv);
		break;
	}
	return (0);
}
