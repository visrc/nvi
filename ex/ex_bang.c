/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 5.23 1993/01/23 16:30:56 bostic Exp $ (Berkeley) $Date: 1993/01/23 16:30:56 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *	Pass the rest of the line after the ! character to the
 *	program named by the SHELL environment variable.
 */
int
ex_bang(cmdp)
	EXCMDARG *cmdp;
{
	static u_char *lastcom;
	register int ch, len, modified;
	register u_char *p, *t;
	u_char *com;

	/* Make sure we got something. */
	if (cmdp->string == NULL) {
		msg("Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Figure out how much space we could possibly need. */
	modified = 0;
	len = USTRLEN(cmdp->string) + 1;
	for (p = cmdp->string; p = USTRPBRK(p, "!%#\\"); ++p)
		switch (*p) {
		case '!':
			if (lastcom == NULL) {
				msg("No previous command to replace \"!\".");
				return (1);
			}
			len += USTRLEN(lastcom);
			modified = 1;
			break;
		case '%':
			if (FF_ISSET(curf, F_NONAME)) {
				msg("No filename to substitute for %%.");
				return (1);
			}
			len += curf->nlen;
			modified = 1;
			break;
		case '#':
			if (FF_ISSET(curf, F_NONAME)) {
				msg("No filename to substitute for #.");
				return (1);
			}
			len += curf->nlen;
			modified = 1;
			break;
		case '\\':
			if (*p)
				++p;
			break;
		}

	/* Allocate space. */
	if ((com = malloc(len)) == NULL) {
		msg("Error: %s", strerror(errno));
		return (1);
	}

	/* Fill it in. */
	if (modified) {
		for (p = cmdp->string, t = com; ch = *p; ++p)
			switch (ch) {
			case '!':
				len = USTRLEN(lastcom);
				memmove(t, lastcom, len);
				t += len;
				break;
			case '%':
				memmove(t, curf->name, curf->nlen);
				t += curf->nlen;
				break;
			case '#':
				memmove(t, curf->name, curf->nlen);
				t += curf->nlen;
				break;
			case '\\':
				if (*p)
					ch = *++p;
				/* FALLTHROUGH */
			default:
				*t++ = ch;
			}
		*p = '\0';
	} else
		memmove(com, cmdp->string, len);
		

	/* Swap commands. */
	if (lastcom)
		free(lastcom);
	lastcom = com;

	MODIFY_WARN(curf);

	/* If modified, echo the new command. */
	if (modified)
		(void)fprintf(curf->stdfp, "%s\n", com);

	/*
	 * If no addresses were specified, just run the command, otherwise
	 * pipe lines from the file through the command.
	 */
	if (cmdp->addrcnt == 0) {
		if (esystem(PVAL(O_SHELL), com))
			return (1);
	} else {
		if (filter(&cmdp->addr1, &cmdp->addr2, com, STANDARD))
			return (1);
		FF_SET(curf, F_AUTOPRINT);
	}
	return (0);
}
