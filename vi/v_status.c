/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.16 1993/02/24 13:03:50 bostic Exp $ (Berkeley) $Date: 1993/02/24 13:03:50 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"

/*
 * v_status --
 *	Show the file status.
 */
int
v_status(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	status(ep, fm->lno);
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
		msg(ep, M_DISPLAY,
		    "%s: %s%s: line %lu of %lu [%ld%%]", ep->name,
		    FF_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified", ro,
		    lno, last, (lno * 100) / last);
	else
		msg(ep, M_DISPLAY,
		    "%s: %s%s: empty file", ep->name,
		    FF_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified", ro);
}
