/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_screen.c,v 5.4 1993/05/11 17:14:12 bostic Exp $ (Berkeley) $Date: 1993/05/11 17:14:12 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

int
ex_split(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXF *tep;
	char *fname;

	if (cmdp->argc) {
		fname = (char *)cmdp->argv[0];
		if (ex_set_altfname(sp, fname))
			return (1);
	} else
		fname = NULL;

	if ((tep = file_get(sp, ep, fname, 1)) == NULL)
		return (1);
	
	return (sp->s_split(sp, tep));
}
