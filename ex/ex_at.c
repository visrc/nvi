/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.2 1992/04/04 10:02:28 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:28 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_at(cmdp)
	CMDARG *cmdp;
{
	static	nest = FALSE;
	int	result;
	char	buf[MAXRCLEN];
	char *extra;

	extra = cmdp->argv[0];
	/* don't allow nested macros */
	if (nest)
	{
		msg("@ macros can't be nested");
		return;
	}
	nest = TRUE;

	/* require a buffer name */
	if (*extra == '"')
		extra++;
	if (!*extra || !isascii(*extra) ||!islower(*extra))
	{
		msg("@ requires a cut buffer name (a-z)");
	}

	/* get the contents of the buffer */
	result = cb2str(*extra, buf, (unsigned)(sizeof buf));
	if (result <= 0)
	{
		msg("buffer \"%c is empty", *extra);
	}
	else if (result >= sizeof buf)
	{
		msg("buffer \"%c is too large to execute", *extra);
	}
	else
	{
		/* execute the contents of the buffer as ex commands */
		exstring(buf, result, 1);
	}

	nest = FALSE;
}
