/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.5 1993/02/19 11:13:14 bostic Exp $ (Berkeley) $Date: 1993/02/19 11:13:14 $";
#endif /* not lint */

#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"

/*
 * ex_init --
 *	Initialize ex.
 */
int
ex_init(ep)
	EXF *ep;
{
	struct termios raw;

	/* Initialize the terminal state. */
	cfmakeraw(&raw);
	raw.c_oflag |= OPOST|ONLCR;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw))
		return (1);

	/* Set up ex functions. */
	ep->s_confirm = ex_confirm;
	ep->s_end = ex_end;

	/* Write to the terminal. */
	ep->stdfp = stdout;

	/* Initialize the terminal size. */
	ep->lines = LVAL(O_LINES);
	ep->cols = LVAL(O_COLUMNS);

	/*
	 * Ex always starts editing at the end of the file;
	 * going to ex from vi retains the current line.
	 */
	if (FF_ISSET(ep, F_NEWSESSION))
		ep->lno = file_lline(ep);
	ep->cno = 0;
	return (0);
}

/*
 * ex_end --
 *	End ex session.
 */
int
ex_end(ep)
	EXF *ep;
{
	extern struct termios original_termios;

	/* Flush any remaining output. */
	(void)fflush(ep->stdfp);

	/* Reset the terminal state. */
	return (tcsetattr(STDIN_FILENO, TCSADRAIN, &original_termios) ? 1 : 0);
}
