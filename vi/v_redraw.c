/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static const char sccsid[] = "$Id: v_redraw.c,v 8.5 1994/08/17 09:51:46 bostic Exp $ (Berkeley) $Date: 1994/08/17 09:51:46 $";
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
v_redraw(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	F_SET(sp, S_REFRESH);
	return (0);
}
