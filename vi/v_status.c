/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.8 1992/05/27 10:38:05 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:38:05 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "extern.h"

/*
 * v_status --
 *	Show the file status.
 */
int
v_status(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long lno;

	lno = file_lline(curf);
	msg("\"%s\" %s%s: line %lu of %lu [%ld%%]",
	    curf->name,
	    curf->flags & F_MODIFIED ? "[MODIFIED]" : "[UNMODIFIED]",
	    curf->flags & F_RDONLY || ISSET(O_READONLY) ? "[READONLY]" : "",
	    fm->lno, lno, (fm->lno * 100) / lno);
	return (1);
}
