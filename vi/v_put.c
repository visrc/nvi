/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 10.3 1995/09/21 10:59:05 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:59:05 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

static void	inc_buf __P((SCR *, VICMD *));

/*
 * v_Put -- [buffer]P
 *	Insert the contents of the buffer before the cursor.
 *
 * PUBLIC: int v_Put __P((SCR *, VICMD *));
 */
int
v_Put(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	u_long cnt;

	if (F_ISSET(vp, VC_ISDOT))
		inc_buf(sp, vp);

	/*
	 * !!!
	 * Historic vi did not support a count with the 'p' and 'P'
	 * commands.  It's useful, so we do.
	 */
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (put(sp, NULL,
		    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
		    &vp->m_start, &vp->m_final, 0))
			return (1);
		vp->m_start = vp->m_final;
		if (INTERRUPTED(sp))
			return (1);
	}
	return (0);
}

/*
 * v_put -- [buffer]p
 *	Insert the contents of the buffer after the cursor.
 *
 * PUBLIC: int v_put __P((SCR *, VICMD *));
 */
int
v_put(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	u_long cnt;

	if (F_ISSET(vp, VC_ISDOT))
		inc_buf(sp, vp);

	/*
	 * !!!
	 * Historic vi did not support a count with the 'p' and 'P'
	 * commands.  It's useful, so we do.
	 */
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (put(sp, NULL,
		    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
		    &vp->m_start, &vp->m_final, 1))
			return (1);
		vp->m_start = vp->m_final;
		if (INTERRUPTED(sp))
			return (1);
	}
	return (0);
}

/*
 * !!!
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
	VICMD *vp;
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
