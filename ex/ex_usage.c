/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_usage.c,v 8.5 1993/10/11 09:38:06 bostic Exp $ (Berkeley) $Date: 1993/10/11 09:38:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * ex_help -- :help
 *	Display help message.
 */
int
ex_help(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	(void)fprintf(sp->stdfp,
	    "To see the list of vi commands, enter \":viusage<CR>\"\n");
	(void)fprintf(sp->stdfp,
	    "To see the list of ex commands, enter \":exusage<CR>\"\n");
	(void)fprintf(sp->stdfp,
	    "For an ex command usage statement enter \":exusage [cmd]<CR>\"\n");
	(void)fprintf(sp->stdfp,
	    "For a vi key usage statement enter \":viusage [key]<CR>\"\n");
	(void)fprintf(sp->stdfp, "To exit, enter \":q!\"\n");
	return (0);
}

/*
 * ex_usage -- :exusage [cmd]
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
		(void)fprintf(sp->stdfp,
		    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
		break;
	case 0:
		for (cp = cmds; cp->name != NULL; ++cp)
			(void)fprintf(sp->stdfp,
			    "%*s: %s\n", MAXCMDNAMELEN, cp->name, cp->help);
		break;
	default:
		abort();
	}
	return (0);
}

/*
 * ex_viusage -- :viusage [key]
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
		(void)fprintf(sp->stdfp,
		    "  Key:%s%s\nUsage: %s\n",
		        isblank(*kp->help) ? "" : " ", kp->help, kp->usage);
		break;
	case 0:
		for (key = 0; key <= MAXVIKEY; ++key) {
			kp = &vikeys[key];
			if (kp->help != NULL)
				(void)fprintf(sp->stdfp, "%s\n", kp->help);
		}
		break;
	default:
		abort();
	}
	return (0);
}
