/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_zexit.c,v 5.10 1992/11/06 18:06:51 bostic Exp $ (Berkeley) $Date: 1992/11/06 18:06:51 $";
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

	SETCMDARG(cmd, C_WQ, 0, OOBLNO, OOBLNO, 1, NULL);
	return (ex_wq(&cmd));
}
