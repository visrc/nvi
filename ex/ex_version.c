/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_version.c,v 10.6 1995/07/04 12:42:23 bostic Exp $ (Berkeley) $Date: 1995/07/04 12:42:23 $";
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
	static const time_t then = 802720593;
	char buf[256];

	(void)strftime(buf, sizeof(buf), "%D", localtime(&then));
	(void)ex_printf(sp,
	    "Version 1.50 (%s) The CSRG, University of California, Berkeley\n",
	    buf);
	return (0);
}
