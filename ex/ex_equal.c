/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 8.5 1994/03/08 19:39:19 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:39:19 $";
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
 * ex_equal -- :address =
 *	Print out the line number matching the specified address, or the
 *	last line number in the file if no address specified.
 */
int
ex_equal(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	if (F_ISSET(cmdp, E_ADDRDEF)) {
		if (file_lline(sp, ep, &lno))
			return (1);
		(void)ex_printf(EXCOOKIE, "%ld\n", lno);
	} else
		(void)ex_printf(EXCOOKIE, "%ld\n", cmdp->addr1.lno);
	return (0);
}
