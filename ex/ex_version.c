/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 10.19 1996/02/26 13:53:02 bostic Exp $ (Berkeley) $Date: 1996/02/26 13:53:02 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#endif

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#ifndef TM_IN_SYS_TIME
#include <time.h>
#endif

#include "../common/common.h"

/*
 * ex_version -- :version
 *	Display the program version.
 *
 * PUBLIC: int ex_version __P((SCR *, EXCMD *));
 */
int
ex_version(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	static const time_t then = 817425060;
	struct tm *t;

	t = localtime(&then);
	(void)ex_printf(sp,
"Version 1.56 (%02d/%02d/%d) The CSRG, University of California, Berkeley\n",
	    t->tm_mon + 1, t->tm_mday, t->tm_year + 1900);
	return (0);
}
