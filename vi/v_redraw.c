/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 10.2 1995/05/05 18:55:35 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:55:35 $";
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
 * v_redraw -- ^R
 *	Redraw the screen.
 *
 * PUBLIC: int v_redraw __P((SCR *, VICMD *));
 */
int
v_redraw(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	F_SET(sp, S_SCR_REFRESH);
	return (0);
}
