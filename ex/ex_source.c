/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 9.1 1994/11/09 18:41:07 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:41:07 $";
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

/*
 * ex_source -- :source file
 *	Execute ex commands from a file.
 */
int
ex_source(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	/*
	 * !!!
	 * :source did not historically set the alternate file name.
	 */
	return (ex_cfile(sp,
	    cmdp->argv[0]->bp, EXPAR_BLIGNORE | EXPAR_VLITONLY));
}
