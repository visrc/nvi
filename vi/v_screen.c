/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_screen.c,v 10.3 1995/06/09 12:52:28 bostic Exp $ (Berkeley) $Date: 1995/06/09 12:52:28 $";
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

#include "common.h"
#include "vi.h"

/*
 * v_screen -- ^W
 *	Switch screens.
 *
 * PUBLIC: int v_screen __P((SCR *, VICMD *));
 */
int
v_screen(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	/*
	 * Try for the next lower screen, or, go back to the first
	 * screen on the stack.
	 */
	if (sp->q.cqe_next != (void *)&sp->gp->dq)
		sp->nextdisp = sp->q.cqe_next;
	else if (sp->gp->dq.cqh_first == sp) {
		msgq(sp, M_ERR, "187|No other screen to switch to");
		return (1);
	} else
		sp->nextdisp = sp->gp->dq.cqh_first;

	F_SET(sp, S_SSWITCH);
	return (0);
}
