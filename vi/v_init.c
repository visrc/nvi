/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 5.7 1993/01/17 16:59:57 bostic Exp $ (Berkeley) $Date: 1993/01/17 16:59:57 $";
#endif /* not lint */

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"

/*
 * v_init --
 *	Initialize vi.
 */
int
v_init(ep)
	EXF *ep;
{
	ep->s_confirm = v_confirm;
	ep->s_end = v_end;

	ep->stdfp = fwopen(ep, v_exwrite);

	if (ISSET(O_COMMENT) && v_comment(ep))
		return (1);

	FF_SET(ep, F_REDRAW);

	ep->lines = LINES;		/* XXX: Way ugly. */
	ep->cols = COLS;

	scr_init(ep);

	return (0);
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(ep)
	EXF *ep;
{
	(void)fclose(ep->stdfp);
	ep->stdfp = stdout;

	scr_end(ep);

	return (0);
}
