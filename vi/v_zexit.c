/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_zexit.c,v 8.5 1993/12/10 12:22:17 bostic Exp $ (Berkeley) $Date: 1993/12/10 12:22:17 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * v_exit -- ZZ
 *	Save the file and exit.
 */
int
v_exit(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (F_ISSET(ep, F_MODIFIED) &&
	    file_write(sp, ep, NULL, NULL, NULL, FS_ALL))
		return (1);

	/*
	 * !!!
	 * Historic practice: quit! or two quit's done in succession
	 * (where ZZ counts as a quit) didn't check for other files.
	 *
	 * Also check for related screens; if they exist, quit, the
	 * user will get the message on the last screen.
	 */
	if (sp->ccnt != sp->q_ccnt + 1 &&
	    ep->refcnt <= 1 && file_unedited(sp) != NULL) {
		sp->q_ccnt = sp->ccnt;
		msgq(sp, M_ERR,
		    "More files to edit; use \":n\" to go to the next file");
		return (1);
	}

	F_SET(sp, S_EXIT);
	return (0);
}
