/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 10.3 1995/06/08 19:02:03 bostic Exp $ (Berkeley) $Date: 1995/06/08 19:02:03 $";
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

#include "common.h"
#include "vi.h"

/*
 * v_status -- ^G
 *	Show the file status.
 *
 * PUBLIC: int v_status __P((SCR *, VICMD *));
 */
int
v_status(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	return (msg_status(sp, vp->m_start.lno, 1));
}
