/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 8.5 1993/09/10 10:04:31 bostic Exp $ (Berkeley) $Date: 1993/09/10 10:04:31 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

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

	/*
	 * ^G in historic vi reset the cursor column to the first
	 * non-blank character in the line.  This doesn't seem of
	 * any usefulness whatsoever, so I don't bother.
	 */
	return (status(sp, ep, fm->lno, 1));
}

int
status(sp, ep, lno, showlast)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	int showlast;
{
	recno_t last;
	char *mo, *ro, *pid;
#ifdef DEBUG
	char pbuf[50];

	(void)snprintf(pbuf, sizeof(pbuf), " (pid %u)", getpid());
	pid = pbuf;
#else
	pid = "";
#endif
	ro = F_ISSET(sp->frp, FR_RDONLY) ? ", readonly" : "";
	mo = F_ISSET(ep, F_MODIFIED) ? "modified" : "unmodified";
	if (showlast) {
		if (file_lline(sp, ep, &last))
			return (1);
		if (last >= 1)
			msgq(sp, M_INFO, "%s: %s%s: line %lu of %lu [%ld%%]%s",
			    sp->frp->fname, mo, ro, lno,
			    last, (lno * 100) / last, pid);
		else
			msgq(sp, M_INFO, "%s: %s%s: empty file%s",
			    sp->frp->fname, mo, ro, pid);
	} else
		msgq(sp, M_INFO, "%s: %s%s: line %lu%s", sp->frp->fname,
		    mo, ro, lno, pid);
	return (0);
}
