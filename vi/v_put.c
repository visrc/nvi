/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 5.5 1992/05/21 12:59:39 bostic Exp $ (Berkeley) $Date: 1992/05/21 12:59:39 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "extern.h"

/*
 * v_Put -- [buffer]P
 *	Insert the contents of the buffer before the cursor.
 */
int
v_Put(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (put(vp->buffer == OOBCB ? DEFCB : vp->buffer, cp, rp, 0));
}

/*
 * v_put -- [buffer]p
 *	Insert the contents of the buffer after the cursor.
 */
int
v_put(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (put(vp->buffer == OOBCB ? DEFCB : vp->buffer, cp, rp, 1));
}
