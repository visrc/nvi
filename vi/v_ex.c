/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 8.2 1994/02/26 17:13:04 bostic Exp $ (Berkeley) $Date: 1994/02/26 17:13:04 $";
#endif /* not lint */

#include <sys/types.h>

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
