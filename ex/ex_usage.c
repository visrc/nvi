/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_usage.c,v 8.8 1993/11/28 19:32:07 bostic Exp $ (Berkeley) $Date: 1993/11/28 19:32:07 $";
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
	(void)ex_printf(EXCOOKIE,
	    "To see the list of vi commands, enter \":viusage<CR>\"\n");
	(void)ex_printf(EXCOOKIE,
	    "To see the list of ex commands, enter \":exusage<CR>\"\n");
	(void)ex_printf(EXCOOKIE,
	    "For an ex command usage statement enter \":exusage [cmd]<CR>\"\n");
	(void)ex_printf(EXCOOKIE,
	    "For a vi key usage statement enter \":viusage [key]<CR>\"\n");
	(void)ex_printf(EXCOOKIE, "To exit, enter \":q!\"\n");
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
		if (cp->name == NULL)
			(void)ex_printf(EXCOOKIE,
			    "The %.*s command is unknown.", (int)len, p);
		else
			(void)ex_printf(EXCOOKIE,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
		break;
	case 0:
		for (cp = cmds; cp->name != NULL; ++cp)
			(void)ex_printf(EXCOOKIE,
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
		if (kp->func == NULL)
nokey:			(void)ex_printf(EXCOOKIE,
			    "The %s key has no current meaning",
			    charname(sp, key));
		else
			(void)ex_printf(EXCOOKIE,
			    "  Key:%s%s\nUsage: %s\n",
			    isblank(*kp->help) ? "" : " ", kp->help, kp->usage);
		break;
	case 0:
		for (key = 0; key <= MAXVIKEY; ++key) {
			kp = &vikeys[key];
			if (kp->help != NULL)
				(void)ex_printf(EXCOOKIE, "%s\n", kp->help);
		}
		break;
	default:
		abort();
	}
	return (0);
}
