/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ex.c,v 5.52 1993/05/15 10:55:21 bostic Exp $ (Berkeley) $Date: 1993/05/15 10:55:21 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_ex --
 *	Run ex.
 */
int
v_ex(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (sp->s_ex_run(sp, ep, rp));
}
