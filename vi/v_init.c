/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_init.c,v 5.1 1992/11/03 15:37:49 bostic Exp $ (Berkeley) $Date: 1992/11/03 15:37:49 $";
#endif /* not lint */

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

/*
 * v_init --
 *	Initialize vi.
 */
int
v_init(ep)
	EXF *ep;
{
	static int first = 1;

	if (first) {
		first = 0;
		scr_init(ep);
	}

	ep->lines = LINES;
	ep->cols = COLS;

	ep->s_confirm = v_confirm;

	ep->stdfp = fwopen(ep, v_exwrite);

	scr_ref(ep);

	MOVE(0, 0);
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
	(void)scr_end(ep);

	return (0);
}
