/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.4 1992/04/19 08:53:50 bostic Exp $ (Berkeley) $Date: 1992/04/19 08:53:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_file(cmdp)
	EXCMDARG *cmdp;
{
	switch(cmdp->argc) {
	case 0:
		break;
	case 1:
		(void)strcpy(origname, cmdp->argv[0]);
		storename(origname);
		break;
	default:
		msg("Usage: file [newname].");
		return (1);
	}

	msg("\"%s\" %s%s %ld lines,  line %ld [%ld%%]",
		*origname ? origname : "[UNNAMED FILE]",
		tstflag(file, MODIFIED) ? "[MODIFIED]" : "",
		tstflag(file, READONLY) ? "[READONLY]" : "",
		nlines,
		markline(cmdp->addr1),
		markline(cmdp->addr1) * 100 / nlines);
	return (0);
}
