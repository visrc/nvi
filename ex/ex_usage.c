/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_usage.c,v 5.3 1993/02/12 14:25:46 bostic Exp $ (Berkeley) $Date: 1993/02/12 14:25:46 $";
#endif /* not lint */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * ex_usage -- :usage cmd
 *	Display ex usage strings.
 */
int
ex_usage(cmdp)
	EXCMDARG *cmdp;
{
	EXCMDLIST *cp;
	size_t len;
	u_char *p;

	
	for (cp = cmds, p = cmdp->argv[0], len = USTRLEN(p);
	    cp->name && memcmp(p, cp->name, len); ++cp);
	if (cp->name == NULL) {
		msg("The %.*s command is unknown.", len, p);
		return (1);
	}
	(void)fprintf(curf->stdfp, "Usage: %s", cp->usage);
	return (0);
}

/*
 * ex_viusage -- :viusage key
 *	Display vi usage strings.
 */
int
ex_viusage(cmdp)
	EXCMDARG *cmdp;
{
	VIKEYS *kp;

	u_char key;

	key = *(u_char *)cmdp->argv[0];
	if (key > MAXVIKEY)
		goto nokey;

	kp = &vikeys[key];
	if (kp->func == NULL) {
nokey:		msg("The %s key has no current meaning.", asciiname[key]);
		return (1);
	}

	(void)fprintf(curf->stdfp, "Usage: %s", kp->usage);
	return (0);
}
