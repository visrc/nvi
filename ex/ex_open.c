/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_open.c,v 8.1 1993/10/03 14:31:54 bostic Exp $ (Berkeley) $Date: 1993/10/03 14:31:54 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_open -- :[line] o[pen] [/pattern/] [flags]
 *
 *	Switch to single line "open" mode.
 */
int
ex_open(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	/* If open option off, disallow visual command. */
	msgq(sp, M_ERR, "The open command is not yet implemented.");
	return (1);
}
