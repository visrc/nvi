/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 5.16 1993/04/20 18:44:27 bostic Exp $ (Berkeley) $Date: 1993/04/20 18:44:27 $";
#endif /* not lint */

#include <sys/types.h>

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
	/*
	 * Historical whackadoo.  The dot command `puts' the numbered buffer
	 * after the last one put.  For example, `"4p .' would put buffer #4
	 * and buffer #5.  If the user continued to enter '.', the #9 buffer
	 * would be repeatedly output.  This was not documented, and is a bit
	 * tricky to reconstruct.  Historical versions of vi also dropped the
	 * contents of the default buffer after each put, so after `"4p' the
	 * default buffer would be empty.  This makes no sense to me, so we
	 * don't bother.
	 *
	 * This code assumes sequential order of numeric characters.
	 */
	if (F_ISSET(vp, VC_ISDOT) && vp->buffer >= '1' && vp->buffer <= '8')
		++vp->buffer;

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
	if (F_ISSET(vp, VC_ISDOT) && vp->buffer >= '1' && vp->buffer <= '8')
		++vp->buffer;

	return (put(sp, ep, VICB(vp), fm, rp, 1));
}
