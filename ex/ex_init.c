/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.11 1993/03/28 19:05:21 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:05:21 $";
#endif /* not lint */

#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_init --
 *	Initialize ex.
 */
int
ex_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct termios raw;

	/* Initialize the terminal state. */
	if (tcgetattr(STDIN_FILENO, &raw))
		return (1);
	cfmakeraw(&raw);
	raw.c_oflag |= OPOST|ONLCR;
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw))
		return (1);

	/*
	 * Ex always starts editing at the end of the file;
	 * going to ex from vi retains the current line.
	 */
	if (F_ISSET(ep, F_NEWSESSION))
		ep->lno = file_lline(sp, ep);
	ep->cno = 0;
	return (0);
}

/*
 * ex_end --
 *	End ex session.
 */
int
ex_end(sp)
	SCR *sp;
{
	/* Reset the terminal state. */
	return (tcsetattr(STDIN_FILENO,
	    TCSADRAIN, &sp->gp->original_termios) ? 1 : 0);
}
