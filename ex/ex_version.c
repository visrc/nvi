/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ex_version.c,v 10.23 1996/03/22 11:51:50 bostic Exp $ (Berkeley) $Date: 1996/03/22 11:51:50 $";
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
	static const time_t then = 827513488;

	struct tm *t;

	t = localtime(&then);
	(void)ex_printf(sp,
"Version 1.59 (%02d/%02d/%d) The CSRG, University of California, Berkeley\n",
	    t->tm_mon + 1, t->tm_mday, t->tm_year + 1900);
	return (0);
}
