/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.11 1992/10/24 14:24:56 bostic Exp $ (Berkeley) $Date: 1992/10/24 14:24:56 $";
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

	ro = ep->flags & F_RDONLY || ISSET(O_READONLY) ? ", readonly" : "";
	if ((last = file_lline(ep)) >= 1)
		msg("%s: %s%s: line %lu of %lu [%ld%%]", ep->name,
		    ep->flags & F_MODIFIED ? "modified" : "unmodified", ro,
		    lno, last, (lno * 100) / last);
	else
		msg("%s: %s%s: empty file", ep->name,
		    ep->flags & F_MODIFIED ? "modified" : "unmodified", ro);
}
