/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 5.4 1992/05/15 11:14:14 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:14 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_mark -- m[a-z]
 *	Define a mark.
 */
int
v_mark(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (mark_set(vp->character, cp));
}

/*
 * v_markbt -- '['`a-z]
 *	Move to a mark.
 */
int
v_markbt(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK *mp;

	if ((mp = mark_get(vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}

/*
 * v_marksq -- '['`a-z]
 *	Move to the first nonblank character of a line containing a mark.
 */
int
v_marksq(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK *mp;

	if ((mp = mark_get(vp->character)) == NULL)
		return (1);
	*rp = *mp;
	return (v_nonblank(rp));
}
