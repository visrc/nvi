/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 8.6 1993/08/25 16:45:42 bostic Exp $ (Berkeley) $Date: 1993/08/25 16:45:42 $";
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
	 * The default address is the last line of the file.  If the address
	 * set bit is on for this file, load the address, ensuring that it
	 * exists.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET)) {
		sp->lno = sp->frp->lno;
		sp->cno = sp->frp->cno;

		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (file_lline(sp, ep, &sp->lno))
				return (1);
			if (sp->lno == 0)
				sp->lno = 1;
			sp->cno = 0;
		} else if (sp->cno >= len)
			sp->cno = 0;
	} else {
		if (file_lline(sp, ep, &sp->lno))
			return (1);
		if (sp->lno == 0)
			sp->lno = 1;
		sp->cno = 0;
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
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	return (0);
}
