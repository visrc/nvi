/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 5.14 1993/03/25 15:01:20 bostic Exp $ (Berkeley) $Date: 1993/03/25 15:01:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Put -- [buffer]P
 *	Insert the contents of the buffer before the cursor.
 */
int
v_Put(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (put(sp, ep, VICB(vp), fm, rp, 0));
}

/*
 * v_put -- [buffer]p
 *	Insert the contents of the buffer after the cursor.
 */
int
v_put(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (put(sp, ep, VICB(vp), fm, rp, 1));
}
