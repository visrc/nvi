/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.13 1993/04/17 11:59:41 bostic Exp $ (Berkeley) $Date: 1993/04/17 11:59:41 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_init --
 *	Initialize ex.
 */
int
ex_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	recno_t last;
	size_t len;

	/*
	 * If no starting location specified, ex starts at the end.
	 * Otherwise, check to make sure that the location exists.
	 */
	if (F_ISSET(ep, F_NOSETPOS)) {
		sp->lno = file_lline(sp, ep);
		sp->cno = 0;
	} else if (file_gline(sp, ep, ep->lno, &len) == NULL) {
		last = file_lline(sp, ep);
		if (last == 0)
			last = 1;
		if (sp->lno != last || sp->cno != 0) {
			sp->lno = last;
			sp->cno = 0;
			msgq(sp, M_INFO,
			    "Cursor position changed since last edit");
		}
	} else {
		sp->lno = ep->lno;
		if (ep->cno >= len) {
			sp->cno = 0;
			msgq(sp, M_INFO,
			    "Cursor position changed since last edit");
		} else
			sp->cno = ep->cno;
	}

	/*
	 * After location established, run any initial command.  Failure
	 * doesn't halt the session.  Don't worry about the cursor being
	 * repositioned affecting the success of this command, it's
	 * pretty unlikely.
	 */
	if (F_ISSET(ep, F_ICOMMAND)) {
		(void)ex_cstring(sp, ep, ep->icommand, strlen(ep->icommand));
		free(ep->icommand);
		F_CLR(ep, F_ICOMMAND);
	}

	/* Display the status line. */
	status(sp, ep, sp->lno);

	return (0);
}

/*
 * ex_end --
 *	End ex session.
 */
int
ex_end(sp)
	SCR *sp;
{
	/* Save cursor location. */
	sp->ep->lno = sp->lno;
	sp->ep->cno = sp->cno;
	return (0);
}
