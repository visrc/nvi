/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_map.c,v 5.1 1992/04/02 11:21:02 bostic Exp $ (Berkeley) $Date: 1992/04/02 11:21:02 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

void
ex_map(cmdp)
	CMDARG *cmdp;
{
	char	*mapto;
#ifndef NO_FKEY
	static char *fnames[10] =
	{
		"<F10>", "<F1>", "<F2>", "<F3>", "<F4>",
		"<F5>", "<F6>", "<F7>", "<F8>", "<F9>"
	};
	int	key;
#endif
	char *extra;

	extra = cmdp->argv[0];

	/* "map" with no extra will dump the map table contents */
	if (!*extra)
	{
		dumpkey(cmdp->flags & E_FORCE ? WHEN_VIINP|WHEN_VIREP : WHEN_VICMD);
	}
	else
	{
		/* "extra" is key to map, followed by what it maps to */
		for (mapto = extra; *mapto && (*mapto != ' ' && *mapto != '\t' || QTST(mapto)); mapto++)
		{
		}
		while ((*mapto == ' ' || *mapto == '\t') && !QTST(mapto))
		{
			*mapto++ = '\0';
		}

#ifndef NO_FKEY
		/* if the mapped string is a number, then assume that the user
		 * wanted that function key
		 */
		if (extra[0] == '#' && isdigit(extra[1]))
		{
			key = atoi(extra + 1) % 10;
			if (FKEY[key])
				mapkey(FKEY[key], mapto, cmdp->flags & E_FORCE ? WHEN_VIINP|WHEN_VIREP : WHEN_VICMD, fnames[key]);
			else
				msg("This terminal has no %s key", fnames[key]);
		}
		else
#endif
		mapkey(extra, mapto, cmdp->flags & E_FORCE ? WHEN_VIINP|WHEN_VIREP : WHEN_VICMD, (char *)0);
	}
}
