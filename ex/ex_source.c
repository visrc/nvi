/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_source.c,v 5.1 1992/04/02 11:21:14 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:14 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

/* execute EX commands from a file */
void
ex_source(cmdp)
	CMDARG *cmdp;
{
	/* must have a filename */
	if (!*cmdp->argv[0])
	{
		msg("\"source\" requires a filename");
		return;
	}

	exfile(cmdp->argv[0], 1);
}
