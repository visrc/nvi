/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.17 1993/02/28 14:02:01 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:02:01 $";
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
		ep->msg(ep, M_DISPLAY,
		    "%s: %s%s: line %lu of %lu [%ld%%]", ep->name,
		    FF_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified", ro,
		    lno, last, (lno * 100) / last);
	else
		ep->msg(ep, M_DISPLAY,
		    "%s: %s%s: empty file", ep->name,
		    FF_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified", ro);
}
