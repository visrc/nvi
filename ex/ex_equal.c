/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_equal.c,v 10.1 1995/04/13 17:22:08 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:22:08 $";
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
 * ex_equal -- :address =
 */
int
ex_equal(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	recno_t lno;

	NEEDFILE(sp, cmdp->cmd);

	/*
	 * Print out the line number matching the specified address,
	 * or the number of the last line in the file if no address
	 * specified.
	 *
	 * !!!
	 * Historically, ":0=" displayed 0, and ":=" or ":1=" in an
	 * empty file displayed 1.  Until somebody complains loudly,
	 * we're going to do it right.  The tables in excmd.c permit
	 * lno to get away with any address from 0 to the end of the
	 * file, which, in an empty file, is 0.
	 */
	if (F_ISSET(cmdp, E_ADDR_DEF)) {
		if (file_lline(sp, &lno))
			return (1);
	} else
		lno = cmdp->addr1.lno;

	F_SET(sp, S_EX_WROTE);
	(void)ex_printf(EXCOOKIE, "%ld\n", lno);
	return (0);
}
