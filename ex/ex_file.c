/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_file.c,v 5.1 1992/04/02 11:20:56 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:20:56 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_file(cmdp)
	CMDARG *cmdp;
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
		return;
	}

	msg("\"%s\" %s%s %ld lines,  line %ld [%ld%%]",
		*origname ? origname : "[UNNAMED FILE]",
		tstflag(file, MODIFIED) ? "[MODIFIED]" : "",
		tstflag(file, READONLY) ? "[READONLY]" : "",
		nlines,
		markline(cmdp->addr1),
		markline(cmdp->addr1) * 100 / nlines);
}
