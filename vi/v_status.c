/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 8.16 1994/08/14 13:37:49 bostic Exp $ (Berkeley) $Date: 1994/08/14 13:37:49 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
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
 * v_status -- ^G
 *	Show the file status.
 */
int
v_status(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{

	/*
	 * ^G in historic vi reset the cursor column to the first
	 * non-blank character in the line.  This doesn't seem of
	 * any usefulness whatsoever, so I don't bother.
	 */
	return (msg_status(sp, ep, vp->m_start.lno, 1));
}
