/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 5.10 1992/11/01 13:56:38 bostic Exp $ (Berkeley) $Date: 1992/11/01 13:56:38 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_yank -- [count]y[count][motion]
 *	Yank text into a cut buffer.
 */
int
v_yank(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	*rp = *fm;
	return (cut(VICB(vp), fm, tm, vp->flags & VC_LMODE));
}
