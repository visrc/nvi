/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_screen.c,v 8.2 1993/08/05 18:07:24 bostic Exp $ (Berkeley) $Date: 1993/08/05 18:07:24 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_split --	:sp[lit]! [file ...]
 *	Split the screen, optionally setting the file list.
 */
int
ex_split(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (sp->s_split(sp, cmdp->argc ? cmdp->argv : NULL));
}
