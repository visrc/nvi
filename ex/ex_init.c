/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.3 1993/02/14 13:44:44 bostic Exp $ (Berkeley) $Date: 1993/02/14 13:44:44 $";
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

	ep->s_confirm = ex_confirm;
	ep->s_end = ex_end;

	cfmakeraw(&raw);
	raw.c_oflag |= OPOST|ONLCR;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw))
		return (1);

	ep->lines = LVAL(O_LINES);
	ep->cols = LVAL(O_COLUMNS);

	/*
	 * Ex always starts editing at the end of the file;
	 * going to ex from vi retains the current line.
	 */
	if (FF_ISSET(ep, F_NEWSESSION))
		curf->lno = file_lline(curf);
	curf->cno = 0;
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

	return (tcsetattr(STDIN_FILENO, TCSADRAIN, &original_termios) ? 1 : 0);
}
