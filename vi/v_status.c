/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 8.11 1994/02/26 17:20:04 bostic Exp $ (Berkeley) $Date: 1994/02/26 17:20:04 $";
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
v_status(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{

	/*
	 * ^G in historic vi reset the cursor column to the first
	 * non-blank character in the line.  This doesn't seem of
	 * any usefulness whatsoever, so I don't bother.
	 */
	return (status(sp, ep, vp->m_start.lno, 1));
}

int
status(sp, ep, lno, showlast)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	int showlast;
{
	recno_t last;
	char *mo, *nc, *nf, *ro, *pid;
#ifdef DEBUG
	char pbuf[50];

	(void)snprintf(pbuf, sizeof(pbuf), " (pid %u)", getpid());
	pid = pbuf;
#else
	pid = "";
#endif
	/*
	 * See nvi/exf.c:file_init() for a description of how and
	 * when the read-only bit is set.  Possible displays are:
	 *
	 *	new file
	 *	new file, readonly
	 *	[un]modified
	 *	[un]modified, readonly
	 *	name changed, [un]modified
	 *	name changed, [un]modified, readonly
	 *
	 * !!!
	 * The historic display for "name changed" was "[Not edited]".
	 */
	if (F_ISSET(sp->frp, FR_NEWFILE)) {
		F_CLR(sp->frp, FR_NEWFILE);
		nf = "new file";
		mo = nc = "";
	} else {
		nf = "";
		if (sp->frp->cname != NULL) {
			nc = "name changed";
			mo = F_ISSET(ep, F_MODIFIED) ?
			    ", modified" : ", unmodified";
		} else {
			nc = "";
			mo = F_ISSET(ep, F_MODIFIED) ?
			    "modified" : "unmodified";
		}
	}
	ro = F_ISSET(sp->frp, FR_RDONLY) ? ", readonly" : "";
	if (showlast) {
		if (file_lline(sp, ep, &last))
			return (1);
		if (last >= 1)
			msgq(sp, M_INFO,
			    "%s: %s%s%s%s: line %lu of %lu [%ld%%]%s",
			    FILENAME(sp->frp), nf, nc, mo, ro, lno,
			    last, (lno * 100) / last, pid);
		else
			msgq(sp, M_INFO, "%s: %s%s%s%s: empty file%s",
			    FILENAME(sp->frp), nf, nc, mo, ro, pid);
	} else
		msgq(sp, M_INFO, "%s: %s%s%s%s: line %lu%s",
		    FILENAME(sp->frp), nf, nc, mo, ro, lno, pid);
	return (0);
}
