/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_screen.c,v 9.2 1995/01/11 16:22:21 bostic Exp $ (Berkeley) $Date: 1995/01/11 16:22:21 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_screen -- ^W
 *	Switch screens.
 */
int
v_screen(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	/*
	 * Try for the next lower screen, or, go back to the first
	 * screen on the stack.
	 */
	if (sp->q.cqe_next != (void *)&sp->gp->dq)
		sp->nextdisp = sp->q.cqe_next;
	else if (sp->gp->dq.cqh_first == sp) {
		msgq(sp, M_ERR, "186|No other screen to switch to");
		return (1);
	} else
		sp->nextdisp = sp->gp->dq.cqh_first;

	/*
	 * Display the old screen's status line so the user can
	 * find the screen they want.
	 */
	(void)msg_status(sp, vp->m_start.lno, 0);

	/* Save the old screen's cursor information. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	F_SET(sp, S_SSWITCH);
	return (0);
}
