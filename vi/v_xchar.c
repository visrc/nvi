/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_xchar.c,v 5.5 1992/05/17 17:11:54 bostic Exp $ (Berkeley) $Date: 1992/05/17 17:11:54 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "cut.h"
#include "options.h"
#include "extern.h"

#define	EMPTY {								\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("No characters to delete.");			\
	return (1);							\
}

/*
 * v_xchar --
 *	Deletes the character(s) on which the cursor sits.
 */
int
v_xchar(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK fm, tm;
	u_long cnt;
	size_t len;
	char *p;

	EGETLINE(p, cp->lno, len);
	if (len == 0)
		EMPTY;

	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	fm.lno = tm.lno = cp->lno;

	/*
	 * Try deleting from the cursor to the end of the line, w/o moving the
	 * cursor.  If that's not enough, start deleting left.  The historic
	 * version of vi didn't handle this reasonably; "4x" at EOL wasn't the
	 * same as "xxxx" because the left movement of the cursor as part of
	 * the 'x' command wasn't taken into account.
	 */
	if (cnt < len - cp->cno) {
		fm.cno = cp->cno;
		tm.cno = cp->cno + cnt;
	} else {
		tm.cno = len;
		cnt -= len - cp->cno;
		if (cnt > cp->cno)
			fm.cno = 0;
		else
			fm.cno = cp->cno - cnt;
	}

	if (cut(vp->buffer == OOBCB ? DEFCB : vp->buffer, &fm, &tm, 0) ||
	    delete(&fm, &tm, 0))
		return (1);

	*rp = fm;
	return (0);
}

/*
 * v_Xchar --
 *	Deletes the character(s) immediately before the current cursor
 *	position.
 */
int
v_Xchar(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK fm, tm;
	u_long cnt;
	size_t len;
	char *p;

	if (cp->cno == 0)
		EMPTY;
	EGETLINE(p, cp->lno, len);
	if (len == 0)
		EMPTY;

	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	fm.lno = tm.lno = cp->lno;
	fm.cno = cnt >= cp->cno ? 0 : cp->cno - cnt;
	tm.cno = cp->cno;

	if (cut(vp->buffer == OOBCB ? DEFCB : vp->buffer, &fm, &tm, 0) ||
	    delete(&fm, &tm, 0))

	rp->lno = fm.lno;
	rp->cno = fm.cno;
	return (0);
}
