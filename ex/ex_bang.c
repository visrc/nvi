/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 5.7 1992/04/18 09:54:08 bostic Exp $ (Berkeley) $Date: 1992/04/18 09:54:08 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "options.h"
#include "excmd.h"
#include "extern.h"

/*
 * ex_bang (:[line [,line]] ! command)
 *	Pass the rest of the line after the ! character to the
 *	program named by the SHELL environment variable.
 */
int
ex_bang(cmdp)
	CMDARG *cmdp;
{
	static char *lastcom;
	register int ch, l_alt, l_cur, l_last, len;
	register char *p, *t;
	char *com;

	/* Make sure we got something. */
	if (cmdp->string == NULL) {
		msg("Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Figure out how much space we could possibly need. */
	l_alt = l_cur = l_last = 0;
	len = strlen(cmdp->string) + 1;
	for (p = cmdp->string; p = strpbrk(p, "!%#\\"); ++p)
		switch (*p) {
		case '!':
			if (lastcom == NULL) {
				msg("No previous command to replace \"!\".");
				return (1);
			}
			len += l_last ? l_last : (l_last = strlen(lastcom));
			break;
		case '%':
			if (!*origname) {
				msg("No filename to substitute for %%.");
				return (1);
			}
			len += l_cur ? l_cur : (l_cur = strlen(origname));
			break;
		case '#':
			if (!*prevorig) {
				msg("No filename to substitute for %%.");
				return (1);
			}
			len += l_alt ? l_alt : (l_alt = strlen(origname));
			break;
		case '\\':
			if (*p)
				++p;
			break;
		}

	/* Allocate it. */
	if ((com = malloc(len)) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}

	/* Fill it in. */
	for (p = cmdp->string, t = com; ch = *p; ++p)
		switch (ch) {
		case '!':
			bcopy(lastcom, t, l_last);
			t += l_last;
			break;
		case '%':
			bcopy(origname, t, l_cur);
			t += l_cur;
			break;
		case '#':
			bcopy(prevorig, t, l_alt);
			t += l_alt;
			break;
		case '\\':
			if (*p)
				ch = *++p;
			/* FALLTHROUGH */
		default:
			*t++ = ch;
		}
 	*p = '\0';

	/* Swap commands. */
	if (lastcom)
		free(lastcom);
	lastcom = com;

	/*
	 * If autowrite set, write the file; otherwise warn the user if
	 * the file has been modified but not written.
	 */
	if (ISSET(O_AUTOWRITE)) {
		if (tmpsave(NULL, 0))
			return (1);
	} else if (ISSET(O_WARN) && tstflag(file, MODIFIED)) {
		if (mode == MODE_VI)
			mode = MODE_COLON;
		msg("Warning: the file has been modified but not written.");
	}

	suspend_curses();

	/*
	 * If no addresses were specified, just run the command, otherwise
	 * pipe lines from the file through the command.
	 */
	if (cmdp->addrcnt == 0)
		system(com);
	else {
		filter(cmdp->addr1, cmdp->addr2, com, STANDARD);
		autoprint = 1;
	}

	/* Resume curses quietly for MODE_EX, otherwise noisily. */
	resume_curses(mode == MODE_EX);
	return (0);
}
