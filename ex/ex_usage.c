/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_usage.c,v 8.4 1993/10/10 18:19:41 bostic Exp $ (Berkeley) $Date: 1993/10/10 18:19:41 $";
#endif /* not lint */

#include <sys/types.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * ex_usage -- :usage cmd
 *	Display ex usage strings.
 */
int
ex_usage(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	EXCMDLIST const *cp;
	size_t len;
	char *p;
	
	switch (cmdp->argc) {
	case 1:
		for (cp = cmds, p = cmdp->argv[0], len = strlen(p);
		    cp->name != NULL && memcmp(p, cp->name, len); ++cp);
		if (cp->name == NULL) {
			msgq(sp, M_ERR, "The %.*s command is unknown.", len, p);
			return (1);
		}
		(void)fprintf(sp->stdfp, "Usage: %s\n", cp->usage);
		break;
	case 0:
		for (cp = cmds; cp->name != NULL; ++cp)
			(void)fprintf(sp->stdfp, "%s\n", cp->usage);
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * ex_viusage -- :viusage key
 *	Display vi usage strings.
 */
int
ex_viusage(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	VIKEYS const *kp;
	int key;

	switch (cmdp->argc) {
	case 1:
		key = cmdp->argv[0][0];
		if (key > MAXVIKEY)
			goto nokey;

		/* Special case: '[' and ']' commands. */
		if ((key == '[' || key == ']') && cmdp->argv[0][1] != key)
			goto nokey;

		kp = &vikeys[key];
		if (kp->func == NULL) {
nokey:			msgq(sp, M_ERR, "The %s key has no current meaning",
			    charname(sp, key));
			return (1);
		}
		(void)fprintf(sp->stdfp, "Usage: %s\n", kp->usage);
		break;
	case 0:
		for (key = 0; key <= MAXVIKEY; ++key) {
			kp = &vikeys[key];
			if (kp->func == NULL)
				continue;
			(void)fprintf(sp->stdfp, "%s\n", kp->usage);
		}
		break;
	default:
		abort();
	}
	return (0);
}
