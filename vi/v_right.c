/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_right.c,v 5.3 1992/05/27 10:37:42 bostic Exp $ (Berkeley) $Date: 1992/05/27 10:37:42 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

/*
 * Historic vi allowed "dl" when the cursor was on the last column, deleting
 * the last character, and similarly allowed "dw" when the cursor was on the
 * last column of the file.  It didn't allow "dh" when the cursor was on
 * column 1, although these cases are not strictly analogous.  The point is
 * that some movements would succeed if they were associated with a motion
 * command, and fail otherwise.  This is part of the off-by-1 schizophrenia
 * that plagued vi.  Other examples are that "dfb" deleted everything up to
 * and including the next 'b' character, but "d/b" only deleted everything
 * up to the next 'b' character.  This implementation regularizes the
 * interface.  If the movement succeeds, the associated action will involve
 * every character up to the end of the movement, but not the character moved
 * to.  Trying to get this to be completely backward compatible with the old
 * vi would be extremely time consuming and stupid.  Of course, there are a
 * couple of special cases!  Two, to be exact.  The first is the $ command,
 * which moves to one space beyond the end of line if we're doing a motion
 * command.  The second is the foward word commands which do the same thing.
 */

#define	EOLERR {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("Already at the end of the line.");			\
	return (1);							\
}

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 */
int
v_right(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;
	size_t len;
	char *p;

	EGETLINE(p, fm->lno, len);

	if (len == 0 || fm->cno == len - 1)
		EOLERR;

	cnt = vp->flags & VC_C1SET ? vp->count : 1;

	rp->lno = fm->lno;
	rp->cno = fm->cno + cnt;
	if (rp->cno > len - 1)
		rp->cno = len - 1;
	return (0);
}

/*
 * v_eol -- $
 *	Move to the last column.
 *
 *	One of places that you are allowed to move beyond the end of
 *	the line.
 */
int
v_eol(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	char *p;

	EGETLINE(p, fm->lno, len);

	if (len == 0)
		EOLERR;

	rp->lno = fm->lno;
	rp->cno = vp->flags & VC_ISMOTION ? len : len - 1;
	return (0);
}
