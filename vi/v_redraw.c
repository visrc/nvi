/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_redraw.c,v 5.10 1992/10/18 13:09:19 bostic Exp $ (Berkeley) $Date: 1992/10/18 13:09:19 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>

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
	u_char *p;

	TRACE("cursor: line %lu col %u", fm->lno, fm->cno);
	if ((p = file_gline(curf, fm->lno, &len)) == NULL)
		TRACE(" len: %u {%.*s}\n", len, MIN(len, 20), p);
	else
		TRACE(" GET failed.\n");
#endif
	wrefresh(curscr);
	return (1);
}
