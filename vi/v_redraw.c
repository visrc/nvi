/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.8 1992/06/08 09:26:49 bostic Exp $ (Berkeley) $Date: 1992/06/08 09:26:49 $";
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

	TRACE("cursor: line %lu col %u", fm->lno, fm->cno);
	GETLINE(p, fm->lno, len);
	if (p)
		TRACE(" len: %u {%.*s}\n", len, MIN(len, 20), p);
	else
		TRACE(" GET failed.\n");
#endif
	wrefresh(curscr);
	return (1);
}
