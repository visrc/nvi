/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 8.1 1993/06/09 22:23:37 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:23:37 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_bang -- :[line [,line]] ! command
 *	Pass the rest of the line after the ! character to
 *	the program named by the SHELL environment variable.
 */
int
ex_bang(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	register int ch, len, modified;
	register char *p, *t;
	MARK rm;
	char *com;

	/* Make sure we got something. */
	if (cmdp->string == NULL) {
		msgq(sp, M_ERR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Figure out how much space we could possibly need. */
	modified = 0;
	len = strlen(cmdp->string) + 1;
	for (p = cmdp->string; p = strpbrk(p, "!%#\\"); ++p)
		switch (*p) {
		case '!':
			if (sp->lastbcomm == NULL) {
				msgq(sp, M_ERR,
				    "No previous command to replace \"!\".");
				return (1);
			}
			len += strlen(sp->lastbcomm);
			modified = 1;
			break;
		case '%':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			len += ep->nlen;
			modified = 1;
			break;
		case '#':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
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
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
		return (1);
	}

	/* Fill it in. */
	if (modified) {
		for (p = cmdp->string, t = com; ch = *p; ++p)
			switch (ch) {
			case '!':
				len = strlen(sp->lastbcomm);
				memmove(t, sp->lastbcomm, len);
				t += len;
				break;
			case '%':
				memmove(t, ep->name, ep->nlen);
				t += ep->nlen;
				break;
			case '#':
				memmove(t, ep->name, ep->nlen);
				t += ep->nlen;
				break;
			case '\\':
				if (*p)
					ch = *++p;
				/* FALLTHROUGH */
			default:
				*t++ = ch;
			}
		*t = '\0';
	} else
		memmove(com, cmdp->string, len);

	/* Swap commands. */
	if (sp->lastbcomm != NULL)
		free(sp->lastbcomm);
	sp->lastbcomm = com;

	if (modified)
		(void)fprintf(sp->stdfp, "%s\n", com);

	/*
	 * If addresses were specified, pipe lines from the file through
	 * the command.
	 */
	if (cmdp->addrcnt != 0) {
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, com, STANDARD))
			return (1);
		sp->lno = rm.lno;
		F_SET(sp, S_AUTOPRINT);
		return (0);
	}

	/* If no addresses were specified, run the command. */
	AUTOWRITE(sp, ep);

	MODIFY_WARN(sp, ep);

	/* Run the command. */
	if (ex_run_process(sp, com, NULL, NULL, 0))
		return (1);

	/* Ex terminates with a bang. */
	if (F_ISSET(sp, S_MODE_EX))
		(void)fprintf(sp->stdfp, "!\n");

	return (0);
}
