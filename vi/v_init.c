/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 8.10 1993/09/29 16:20:01 bostic Exp $ (Berkeley) $Date: 1993/09/29 16:20:01 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"

/*
 * v_init --
 *	Initialize vi.
 */
int
v_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	size_t len;

	/* Make ex display to a special function. */
	if ((sp->stdfp = fwopen(sp, sp->s_ex_write)) == NULL) {
		msgq(sp, M_ERR, "ex output: %s", strerror(errno));
		return (1);
	}
#ifdef MAKE_EX_OUTPUT_LINE_BUFFERED
	(void)setvbuf(sp->stdfp, NULL, _IOLBF, 0);
#endif

	/*
	 * The default address is line 1, column 0.  If the address set
	 * bit is on for this file, load the address, ensuring that it
	 * exists.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET)) {
		sp->lno = sp->frp->lno;
		sp->cno = sp->frp->cno;

		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (sp->lno != 1 || sp->cno != 0) {
				if (file_lline(sp, ep, &sp->lno))
					return (1);
				if (sp->lno == 0)
					sp->lno = 1;
				sp->cno = 0;
			}
		} else if (sp->cno >= len)
			sp->cno = 0;

	} else {
		sp->lno = 1;
		sp->cno = 0;

		if (O_ISSET(sp, O_COMMENT) && v_comment(sp, ep))
			return (1);
	}

	/* Display the status line. */
	return (status(sp, ep, sp->lno, 0));
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(sp)
	SCR *sp;
{
#ifdef FWOPEN_NOT_AVAILABLE
	sp->trapped_fd = -1;
#endif
	(void)fclose(sp->stdfp);
	return (0);
}
