/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.12 1992/11/07 13:42:48 bostic Exp $ (Berkeley) $Date: 1992/11/07 13:42:48 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
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
	status(curf, fm->lno);
	return (1);
}

void
status(ep, lno)
	EXF *ep;
	recno_t lno;
{
	recno_t last;
	char *ro;

	ro = FF_ISSET(ep, F_RDONLY) || ISSET(O_READONLY) ? ", readonly" : "";
	if ((last = file_lline(ep)) >= 1)
		msg("%s: %s%s: line %lu of %lu [%ld%%]", ep->name,
		    FF_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified", ro,
		    lno, last, (lno * 100) / last);
	else
		msg("%s: %s%s: empty file", ep->name,
		    FF_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified", ro);
}
