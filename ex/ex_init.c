/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.12 1993/04/05 07:11:37 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:37 $";
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
		sp->lno = file_lline(sp, ep);
	sp->cno = 0;
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
