/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_preserve.c,v 8.2 1993/08/05 18:07:44 bostic Exp $ (Berkeley) $Date: 1993/08/05 18:07:44 $";
#endif /* not lint */


#include <sys/types.h>

#include <errno.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "recover.h"

/*
 * ex_preserve -- :pre[serve]
 *	Push the file to recovery.
 */
int
ex_preserve(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	recno_t lno;

	if (!F_ISSET(ep, F_RCV_ON)) {
		msgq(sp, M_ERR, "Preservation of this file not possible.");
		return (1);
	}

	/* Force the file to be read in, in case it hasn't yet. */
	if (file_lline(sp, ep, &lno))
		return (1);

	if (rcv_sync(sp, ep))
		return (1);
	F_SET(ep, F_RCV_NORM);
	msgq(sp, M_INFO, "File preserved.");
	return (0);
}
