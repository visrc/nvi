/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.21 1993/04/06 11:43:55 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:43:55 $";
#endif /* not lint */

#include <sys/types.h>

#include <unistd.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_status --
 *	Show the file status.
 */
int
v_status(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	(void)status(sp, ep, fm->lno);
	return (1);
}

void
status(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	recno_t last;
	char *ro, *pid;
#ifdef DEBUG
	char pbuf[30];

	(void)snprintf(pbuf, sizeof(pbuf), " (%u)", getpid());
	pid = pbuf;
#else
	pid = "";
#endif
	ro = F_ISSET(sp, F_RDONLY) ||
	    O_ISSET(sp, O_READONLY) ? ", readonly" : "";
	if ((last = file_lline(sp, ep)) >= 1)
		msgq(sp, M_DISPLAY,
		    "%s: %s%s: line %lu of %lu [%ld%%]%s", ep->name,
		    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
		    ro, lno, last, (lno * 100) / last, pid);
	else
		msgq(sp, M_DISPLAY,
		    "%s: %s%s: empty file%s", ep->name,
		    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
		    ro, pid);
}
