/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_right.c,v 5.1 1992/05/15 11:15:20 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:15:20 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 */
int
v_right(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	EGETLINE(p, cp->lno, len);
	--len;

	if (cp->cno == len) {
		bell();
		if (ISSET(O_VERBOSE))
			msg("Already at the end of the line.");
		return (1);
	}

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	rp->lno = cp->lno;
	if ((cp->cno += cnt) > len)
		rp->cno = len;
	return (0);
}

/*
 * v_eol -- $
 *	Move to the last column.
 */
int
v_eol(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	size_t len;
	char *p;

	EGETLINE(p, cp->lno, len);

	rp->lno = cp->lno;
	rp->cno = len ? len - 1 : 0;
	return (0);
}
