/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.16 1993/05/14 16:38:51 bostic Exp $ (Berkeley) $Date: 1993/05/14 16:38:51 $";
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
	size_t len;

	/*
	 * If no starting location specified, ex starts at the end.
	 * Otherwise, check to make sure that the location exists.
	 */
	if (F_ISSET(ep, F_NOSETPOS)) {
		if ((sp->lno = file_lline(sp, ep)) == 0)
			sp->lno = 1;
		sp->cno = 0;
		F_CLR(sp, F_NOSETPOS);
	} else {
		sp->lno = ep->lno;
		sp->cno = ep->cno;
		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if ((sp->lno = file_lline(sp, ep)) == 0)
				sp->lno = 1;
			sp->cno = 0;
		} else if (sp->cno >= len)
			sp->cno = 0;
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
	return (status(sp, ep, sp->lno, 0));
}

/*
 * ex_end --
 *	End ex session.
 */
int
ex_end(sp)
	SCR *sp;
{
	/* Save the cursor location. */
	sp->ep->lno = sp->lno;
	sp->ep->cno = sp->cno;

	return (0);
}
