/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_init.c,v 5.1 1993/02/14 12:40:56 bostic Exp $ (Berkeley) $Date: 1993/02/14 12:40:56 $";
#endif /* not lint */

#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

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
	return (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw) ? 1 : 0);
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
