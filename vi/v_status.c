/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.5 1992/05/15 11:14:22 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:22 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_status --
 *	Show the file status.
 */
int
v_status(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	u_long lno;

	lno = file_lline(curf);
	msg("\"%s\" %s%s %ld lines,  line %ld [%ld%%]",
	    curf->name,
	    curf->flags & F_MODIFIED ? "[MODIFIED]" : "[UNMODIFIED]",
	    curf->flags & F_RDONLY ? "[READONLY]" : "",
	    cp->lno, lno, (cp->lno * 100) / lno);
	return (1);
}
