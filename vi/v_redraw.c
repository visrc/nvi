/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 10.5 1995/09/21 12:08:32 bostic Exp $ (Berkeley) $Date: 1995/09/21 12:08:32 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"
#include "vi.h"

/*
 * v_redraw -- ^L, ^R
 *	Redraw the screen.
 *
 * PUBLIC: int v_redraw __P((SCR *, VICMD *));
 */
int
v_redraw(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (sp->gp->scr_refresh(sp, 1));
}
