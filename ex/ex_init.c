/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 8.3 1993/07/06 09:03:51 bostic Exp $ (Berkeley) $Date: 1993/07/06 09:03:51 $";
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
	 * See comment in vi/v_init.c for how this code works.
	 *
	 * The default address is the last line of the file.
	 * If the address is loaded, ensure that it exists.
	 */
	if (F_ISSET(ep, F_EADDR_LOAD)) {
		if (F_ISSET(ep, F_EADDR_NONE)) {
			if (file_lline(sp, ep, &sp->lno))
				return (1);
			if (sp->lno == 0)
				sp->lno = 1;
			sp->cno = 0;
			F_CLR(ep, F_EADDR_LOAD | F_EADDR_NONE);
		} else {
			sp->lno = ep->lno;
			sp->cno = ep->cno;
			F_CLR(ep, F_EADDR_LOAD);
		}
		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (file_lline(sp, ep, &sp->lno))
				return (1);
			if (sp->lno == 0)
				sp->lno = 1;
			sp->cno = 0;
		} else if (sp->cno >= len)
			sp->cno = 0;
		/*
		 * After address set, run any initial command; failure doesn't
		 * halt the session.  Hopefully changing the cursor position
		 * won't affect the success of the command.
		 */
		if (F_ISSET(ep, F_ICOMMAND)) {
			(void)ex_cstring(sp, ep,
			    ep->icommand, strlen(ep->icommand));
			free(ep->icommand);
			F_CLR(ep, F_ICOMMAND);
		}
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
