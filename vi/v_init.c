/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 8.14 1993/11/12 17:00:15 bostic Exp $ (Berkeley) $Date: 1993/11/12 17:00:15 $";
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

	/* Reset strange attraction. */
	sp->rcm = 0;
	sp->rcmflags = 0;

	/* Create the private vi structure. */
	if (VP(sp) == NULL) {
		if ((VP(sp) = malloc(sizeof(VI_PRIVATE))) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			return (1);
		}
		memset(VP(sp), 0, sizeof(VI_PRIVATE));
	}

	/* Make ex display to a special function. */
	if ((sp->stdfp = fwopen(sp, sp->s_ex_write)) == NULL) {
		msgq(sp, M_ERR, "ex output: %s", strerror(errno));
		return (1);
	}
#ifdef MAKE_EX_OUTPUT_LINE_BUFFERED
	(void)setvbuf(sp->stdfp, NULL, _IOLBF, 0);
#endif

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
	/* Close down ex output file descriptor. */
	(void)fclose(sp->stdfp);

	/*
	 * XXX
	 * We can't delete the private VI memory, otherwise we'll lose
	 * the dot/dotmotion and last 'r' character information.  The
	 * correct fix is to have a way to register a "call-me-when-the
	 * screen-is-deleted" function.
	 */
	return (0);
}
