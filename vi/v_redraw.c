/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.7 1992/06/07 16:45:21 bostic Exp $ (Berkeley) $Date: 1992/06/07 16:45:21 $";
#endif /* not lint */

#include <sys/param.h>
#include <curses.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_redraw --
 *	Redraw the screen.
 */
int
v_redraw(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
#if DEBUG && 1
	size_t len;
	char *p;

	EGETLINE(p, fm->lno, len);
	TRACE("cursor: line %lu col %u: len: %u {%.*s}\n",
	    fm->lno, fm->cno, len, MIN(len, 20), p);
#endif
	wrefresh(curscr);
	return (1);
}
