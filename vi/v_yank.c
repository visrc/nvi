/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 5.4 1992/05/21 13:00:54 bostic Exp $ (Berkeley) $Date: 1992/05/21 13:00:54 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "extern.h"

/*
 * v_yank -- [count]y[count][motion]
 *	Yank text into a cut buffer.
 */
int
v_yank(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (cut(vp->buffer == OOBCB ? DEFCB : vp->buffer,
	    cp, &vp->motion, vp->flags & VC_LMODE));
}
