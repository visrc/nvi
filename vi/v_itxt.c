/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.4 1992/05/15 11:14:13 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:13 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

static int insert __P((VICMDARG *, MARK *, MARK *, int));

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 */
int
v_iA(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	size_t len;
	char *p;

	EGETLINE(p, cp->lno, len);
	cp->cno = len;

	return (insert(vp, cp, rp, 0));
}

/*
 * v_ia -- [count]a
 *	Append text to the cursor position.
 */
int
v_ia(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	size_t len;
	char *p;

	EGETLINE(p, cp->lno, len);

	if (len > 0)
		++cp->cno;
	return (insert(vp, cp, rp, 0));
}

/*
 * v_iI -- [count]I
 *	Insert text at the front of the line.
 */
int
v_iI(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK m;
	if (v_zero(vp, cp, &m))
		return (1);
	return (insert(vp, &m, rp, 0));
}

/*
 * v_ii -- [count]i
 *	Insert text at the cursor position.
 */
int
v_ii(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	return (insert(vp, cp, rp, 0));
}

/*
 * v_iO -- [count]O
 *	Insert text above this line.
 */
int
v_iO(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	add(cp, "\n", 1);
	return (insert(vp, cp, rp, 1));
}

/*
 * v_io -- [count]o
 *	Insert text after this line.
 */
int
v_io(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	++cp->lno;
	add(cp, "\n", 1);
	return (insert(vp, cp, rp, 0));
}

/*
 * insert --
 *	Start input mode without deleting anything.
 */
static int
insert(vp, cp, rp, above)
	VICMDARG *vp;
	MARK *cp, *rp;
	int above;		/* New line going above old line? */
{
	MARK m, *mp;
	u_long	cnt;
	int wasdot;

	mp = &m;
	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (wasdot = doingdot; cnt-- > 0; doingdot = 1) {
		mp = input(mp, mp, WHEN_VIINP, above);
		++mp->cno;
	}
	if (mp->cno > 0)
		--mp->cno;
	doingdot = wasdot;

	*rp = m;
	return (0);
}
