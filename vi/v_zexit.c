/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_zexit.c,v 5.9 1992/11/02 22:36:09 bostic Exp $ (Berkeley) $Date: 1992/11/02 22:36:09 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_exit -- ZZ
 *	Save the file and exit.
 */
int
v_exit(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	EXCMDARG cmd;

	v_startex();

	SETCMDARG(cmd, C_WQ, 0, OOBLNO, OOBLNO, 0, NULL);
	(void)ex_wq(&cmd);
	v_leaveex();
	return (1);
}
