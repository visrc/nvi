/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 8.5 1993/11/13 18:01:41 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:01:41 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

static void	inc_buf __P((SCR *, VICMDARG *));

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
	if (F_ISSET(vp, VC_ISDOT))
		inc_buf(sp, vp);
	return (put(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? vp->buffer : DEFCB, fm, rp, 0));
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
	if (F_ISSET(vp, VC_ISDOT))
		inc_buf(sp, vp);

	return (put(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? vp->buffer : DEFCB, fm, rp, 1));
}

/*
 * Historical whackadoo.  The dot command `puts' the numbered buffer
 * after the last one put.  For example, `"4p.' would put buffer #4
 * and buffer #5.  If the user continued to enter '.', the #9 buffer
 * would be repeatedly output.  This was not documented, and is a bit
 * tricky to reconstruct.  Historical versions of vi also dropped the
 * contents of the default buffer after each put, so after `"4p' the
 * default buffer would be empty.  This makes no sense to me, so we
 * don't bother.  Don't assume sequential order of numeric characters.
 *
 * And, if that weren't exciting enough, failed commands don't normally
 * set the dot command.  Well, boys and girls, an exception is that
 * the buffer increment gets done regardless of the success of the put.
 */
static void
inc_buf(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	CHAR_T v;

	switch (vp->buffer) {
	case '1':
		v = '2';
		break;
	case '2':
		v = '3';
		break;
	case '3':
		v = '4';
		break;
	case '4':
		v = '5';
		break;
	case '5':
		v = '6';
		break;
	case '6':
		v = '7';
		break;
	case '7':
		v = '8';
		break;
	case '8':
		v = '9';
		break;
	default:
		return;
	}
	VIP(sp)->sdot.buffer = vp->buffer = v;
}
