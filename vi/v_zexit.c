/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_zexit.c,v 5.1 1992/04/18 19:39:55 bostic Exp $ (Berkeley) $Date: 1992/04/18 19:39:55 $";
#endif /* not lint */

#include <stdio.h>

#include "vi.h"
#include "curses.h"
#include "excmd.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_xit --
 *	Same as the ex command "xit".
 */
/* ARGSUSED */
MARK
v_xit(m, cnt, key)
	MARK	m;	/* ignored */
	long	cnt;	/* ignored */
	int	key;	/* must be a second 'Z' */
{
	CMDARG cmd;
	/* if second char wasn't 'Z', fail */
	if (key != 'Z')
	{
		return MARK_UNSET;
	}

	/* move the cursor to the bottom of the screen */
	move(LINES - 1, 0);
	clrtoeol();

	/* do the xit command */
	SETCMDARG(cmd, C_XIT, 2, m, m, FALSE, "");
	ex_xit(&cmd);

	/* return the cursor */
	return m;
}
