/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 5.14 1992/05/07 12:46:33 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:46:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "exf.h"
#include "options.h"
#include "term.h"
#include "extern.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *	Pass the rest of the line after the ! character to the
 *	program named by the SHELL environment variable.
 */
int
ex_bang(cmdp)
	EXCMDARG *cmdp;
{
	static char *lastcom;
	register int ch, len, modified;
	register char *p, *t;
	EXF *ep;
	char *com;

	/* Make sure we got something. */
	if (cmdp->string == NULL) {
		msg("Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Figure out how much space we could possibly need. */
	modified = 0;
	len = strlen(cmdp->string) + 1;
	for (p = cmdp->string; p = strpbrk(p, "!%#\\"); ++p)
		switch (*p) {
		case '!':
			if (lastcom == NULL) {
				msg("No previous command to replace \"!\".");
				return (1);
			}
			len += strlen(lastcom);
			modified = 1;
			break;
		case '%':
			if (curf->flags & F_NONAME) {
				msg("No filename to substitute for %%.");
				return (1);
			}
			len += curf->nlen;
			modified = 1;
			break;
		case '#':
			if (ep == NULL)
				ep = file_prev(curf);
			if (ep == NULL || ep->flags & F_NONAME) {
				msg("No filename to substitute for #.");
				return (1);
			}
			len += ep->nlen;
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
				len = strlen(lastcom);
				bcopy(lastcom, t, len);
				t += len;
				break;
			case '%':
				bcopy(curf->name, t, curf->nlen);
				t += curf->nlen;
				break;
			case '#':
				bcopy(ep->name, t, ep->nlen);
				t += ep->nlen;
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
		bcopy(cmdp->string, com, len);
		

	/* Swap commands. */
	if (lastcom)
		free(lastcom);
	lastcom = com;

	DEFMODSYNC;

	EX_PRSTART(0);

	/* If modified, echo the new command. */
	if (modified) {
		(void)printf("%s", com);
		EX_PRNEWLINE;
	}

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
		autoprint = 1;
	}
	return (0);
}
