/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 8.3 1994/03/08 19:41:13 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:41:13 $";
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
 * v_ex -- :
 *	Execute a colon command line.
 */
int
v_ex(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (sp->s_ex_run(sp, ep, &vp->m_final));
}

/*
 * v_exmode -- Q
 *	Switch the editor into EX mode.
 */
int
v_exmode(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	sp->saved_vi_mode = F_ISSET(sp, S_VI_CURSES | S_VI_XAW);
	F_CLR(sp, S_SCREENS);
	F_SET(sp, S_EX);
	return (0);
}
