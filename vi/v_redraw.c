/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 9.1 1994/11/09 18:36:14 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:36:14 $";
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
 * v_redraw -- ^R
 *	Redraw the screen.
 */
int
v_redraw(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	F_SET(sp, S_SCR_REFRESH);
	return (0);
}
