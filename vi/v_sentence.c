/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_sentence.c,v 5.1 1992/05/15 11:15:07 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:15:07 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

static char *ptrn = "[\\.?!][ \t]|[\\.?!]$";

/*
 * v_fsentence -- [count])
 *	Move forward count sentences.
 */
int
v_fsentence(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	u_long cnt;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;)
		if ((cp = f_search(cp, ptrn, NULL, 0)) == NULL)
			return (1);
return (0);
/*
	return (v_fword(vp, cp, rp));
*/
}

/*
 * v_bsentence -- [count])
 *	Move forward count sentences.
 */
int
v_bsentence(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	u_long cnt;

	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;)
		if ((cp = b_search(cp, ptrn, NULL, 0)) == NULL)
			return (1);
/*
	return (v_fword(vp, cp, rp));
*/
	return (0);
}
