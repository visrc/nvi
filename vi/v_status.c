/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.23 1993/05/09 11:28:20 bostic Exp $ (Berkeley) $Date: 1993/05/09 11:28:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <unistd.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_status -- ^G
 *	Show the file status.
 */
int
v_status(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	status(sp, ep, fm->lno);

	/*
	 * For some unknown reason, ^G in historic vi reset the cursor
	 * column to the first non-blank character in the line.  This
	 * doesn't seem particular useful, so I don't bother.
	 */
	*rp = *fm;
	return (0);
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
	char pbuf[50];

	(void)snprintf(pbuf, sizeof(pbuf), " (pid %u)", getpid());
	pid = pbuf;
#else
	pid = "";
#endif
	ro = F_ISSET(sp, F_RDONLY) ||
	    O_ISSET(sp, O_READONLY) ? ", readonly" : "";
	if ((last = file_lline(sp, ep)) >= 1)
		msgq(sp, M_INFO,
		    "%s: %s%s: line %lu of %lu [%ld%%]%s", ep->name,
		    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
		    ro, lno, last, (lno * 100) / last, pid);
	else
		msgq(sp, M_INFO,
		    "%s: %s%s: empty file%s", ep->name,
		    F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified",
		    ro, pid);
}
