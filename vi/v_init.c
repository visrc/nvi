/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 9.8 1994/11/25 12:02:36 bostic Exp $ (Berkeley) $Date: 1994/11/25 12:02:36 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

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
		nvip->csearchdir = CNOTSET;
	} else {
		ovip = VIP(orig);

		/* User can replay the last input, but nothing else. */
		if (ovip->rep_len != 0) {
			MALLOC(orig, nvip->rep, CH *, ovip->rep_len);
			if (nvip->rep != NULL) {
				memmove(nvip->rep, ovip->rep, ovip->rep_len);
				nvip->rep_len = ovip->rep_len;
			}
		}

		if (ovip->ps != NULL &&
		    (nvip->ps = strdup(ovip->ps)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}

		nvip->lastckey = ovip->lastckey;
		nvip->csearchdir = ovip->csearchdir;
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
		free(vip->rep);

	if (vip->ps != NULL)
		free(vip->ps);

	/* Free private memory. */
	FREE(vip, sizeof(VI_PRIVATE));
	sp->vi_private = NULL;

	return (0);
}

/*
 * v_init --
 *	Initialize vi.
 */
int
v_init(sp)
	SCR *sp;
{
	recno_t lno;
	size_t len;

	/*
	 * If the first visit to a file, check to see if we're skipping
	 * an initial comment.  Otherwise, make sure that the cursor
	 * position is a legal one.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET))
		if (file_gline(sp, sp->lno, &len) == NULL) {
			if (file_lline(sp, &lno))
				return (1);
			sp->lno = 1;
			sp->cno = 0;
		} else if (sp->cno >= len) {
			sp->cno = 0;
			if (nonblank(sp, sp->lno, &sp->cno))
				return (1);
		}
	else
		if (O_ISSET(sp, O_COMMENT) && ex_comment(sp))
			return (1);

	/* Make ex display to a vi scrolling function. */
	if ((sp->stdfp = fwopen(sp, sp->s_ex_write)) == NULL) {
		msgq(sp, M_SYSERR, "ex output");
		return (1);
	}

	return (0);
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(sp)
	SCR *sp;
{
	/* Reset ex output file descriptor. */
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
		return (v_buildps(sp));
	}
	return (0);
}
