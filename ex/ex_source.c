/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 10.1 1995/04/13 17:22:29 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:22:29 $";
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
 * ex_source -- :source file
 *	Execute ex commands from a file.
 */
int
ex_source(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	/*
	 * !!!
	 * :source did not historically set the alternate file name.
	 */
	return (ex_cfile(sp, cmdp->argv[0]->bp, E_BLIGNORE | E_VLITONLY));
}
