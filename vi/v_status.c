/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 9.3 1995/02/22 00:06:38 bostic Exp $ (Berkeley) $Date: 1995/02/22 00:06:38 $";
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
v_status(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	return (msg_status(sp, vp->m_start.lno, 1));
}
