/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 10.11 1995/09/21 10:58:09 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:58:09 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

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
	static const time_t then = 806770898;
	struct tm *t;

	t = localtime(&then);
	(void)ex_printf(sp,
"Version 1.52 (%02d/%02d/%d) The CSRG, University of California, Berkeley\n",
	    t->tm_mon + 1, t->tm_mday, t->tm_year + 1900);
	return (0);
}
