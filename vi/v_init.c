/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 8.16 1993/12/09 19:43:13 bostic Exp $ (Berkeley) $Date: 1993/12/09 19:43:13 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"

/*
 * v_screen_copy --
 *	Copy vi screen.
 */
int
v_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	VI_PRIVATE *ovip, *nvip;

	/* Create the private vi structure. */
	CALLOC_RET(orig, nvip, VI_PRIVATE *, 1, sizeof(VI_PRIVATE));
	sp->vi_private = nvip;

	if (orig == NULL) {
		nvip->inc_lastch = '+';
		nvip->inc_lastval = 1;
	} else {
		ovip = VIP(orig);

		/* User can replay the last input, but nothing else. */
		if (ovip->rep_len != 0) {
			MALLOC(orig, nvip->rep, char *, ovip->rep_len);
			if (nvip->rep != NULL) {
				memmove(nvip->rep, ovip->rep, ovip->rep_len);
				nvip->rep_len = ovip->rep_len;
			}
		}

		nvip->inc_lastch = ovip->inc_lastch;
		nvip->inc_lastval = ovip->inc_lastval;

		if (ovip->paragraph != NULL &&
		    (nvip->paragraph = strdup(ovip->paragraph)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
	}
	return (0);
}

/*
 * v_screen_end --
 *	End a vi screen.
 */
int
v_screen_end(sp)
	SCR *sp;
{
	VI_PRIVATE *vip;

	vip = VIP(sp);

	if (vip->rep != NULL)
		FREE(vip->rep, vip->rep_len);

	if (vip->paragraph != NULL)
		FREE(vip->paragraph, vip->paragraph_len);

	FREE(vip, sizeof(VI_PRIVATE));
	return (0);
}

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

	/* Make ex display to a special function. */
	if ((sp->stdfp = fwopen(sp, sp->s_ex_write)) == NULL) {
		msgq(sp, M_SYSERR, "ex output");
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

	return (0);
}

/*
 * v_optchange --
 *	Handle change of options for vi.
 */
int
v_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_PARAGRAPHS:
	case O_SECTIONS:
		return (v_buildparagraph(sp));
	}
	return (0);
}
