/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_usage.c,v 10.1 1995/04/13 17:22:36 bostic Exp $ (Berkeley) $Date: 1995/04/13 17:22:36 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "../vi/vi.h"

/*
 * ex_help -- :help
 *	Display help message.
 */
int
ex_help(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	F_SET(sp, S_EX_WROTE);
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
ex_usage(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	ARGS *ap;
	EXCMDLIST const *cp;
	char *name;

	switch (cmdp->argc) {
	case 1:
		F_SET(sp, S_EX_WROTE);
		ap = cmdp->argv[0];
		for (cp = cmds; cp->name != NULL &&
		    memcmp(ap->bp, cp->name, ap->len); ++cp);
		if (cp->name == NULL)
			(void)ex_printf(EXCOOKIE,
			    "The %.*s command is unknown.\n",
			    (int)ap->len, ap->bp);
		else {
			(void)ex_printf(EXCOOKIE,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
			/*
			 * !!!
			 * The "visual" command has two modes, one from ex,
			 * one from the vi colon line.  Don't ask.
			 */
			if (cp != &cmds[C_VISUAL_EX] &&
			    cp != &cmds[C_VISUAL_VI])
				break;
			if (cp == &cmds[C_VISUAL_EX])
				cp = &cmds[C_VISUAL_VI];
			else
				cp = &cmds[C_VISUAL_EX];
			(void)ex_printf(EXCOOKIE,
			    "Command: %s\n  Usage: %s\n", cp->help, cp->usage);
		}
		break;
	case 0:
		for (cp = cmds; cp->name != NULL && !INTERRUPTED(sp); ++cp) {
			/* The ^D command has an unprintable name. */
			if (cp == &cmds[C_SCROLL])
				name = "^D";
			else
				name = cp->name;
			(void)ex_printf(EXCOOKIE,
			    "%*s: %s\n", MAXCMDNAMELEN, name, cp->help);
			F_SET(sp, S_EX_WROTE);
		}
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
ex_viusage(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	VIKEYS const *kp;
	int key;

	switch (cmdp->argc) {
	case 1:
		F_SET(sp, S_EX_WROTE);
		if (cmdp->argv[0]->len != 1) {
			ex_message(sp, cmdp->cmd->usage, EXM_USAGE);
			return (1);
		}
		key = cmdp->argv[0]->bp[0];
		if (key > MAXVIKEY)
			goto nokey;

		/* Special case: '[' and ']' commands. */
		if ((key == '[' || key == ']') && cmdp->argv[0]->bp[1] != key)
			goto nokey;

		/* Special case: ~ command. */
		if (key == '~' && O_ISSET(sp, O_TILDEOP))
			kp = &tmotion;
		else
			kp = &vikeys[key];

		if (kp->func == NULL)
nokey:			(void)ex_printf(EXCOOKIE,
			    "The %s key has no current meaning.\n",
			    KEY_NAME(sp, key));
		else
			(void)ex_printf(EXCOOKIE,
			    "  Key:%s%s\nUsage: %s\n",
			    isblank(*kp->help) ? "" : " ", kp->help, kp->usage);
		break;
	case 0:
		for (key = 0; key <= MAXVIKEY && !INTERRUPTED(sp); ++key) {
			/* Special case: ~ command. */
			if (key == '~' && O_ISSET(sp, O_TILDEOP))
				kp = &tmotion;
			else
				kp = &vikeys[key];
			if (kp->help != NULL)
				(void)ex_printf(EXCOOKIE, "%s\n", kp->help);
			F_SET(sp, S_EX_WROTE);
		}
		break;
	default:
		abort();
	}
	return (0);
}
