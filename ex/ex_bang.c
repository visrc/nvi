/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 5.34 1993/04/05 07:11:26 bostic Exp $ (Berkeley) $Date: 1993/04/05 07:11:26 $";
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
 *	Pass the rest of the line after the ! character to the
 *	program named by the SHELL environment variable.
 */
int
ex_bang(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	struct termios savet;
	register int ch, len, modified;
	register char *p, *t;
	MARK rm;
	int rval;
	char *com;

	/* Make sure we got something. */
	if (cmdp->string == NULL) {
		msgq(sp, M_ERROR, "Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* Figure out how much space we could possibly need. */
	modified = 0;
	len = strlen(cmdp->string) + 1;
	for (p = cmdp->string; p = strpbrk(p, "!%#\\"); ++p)
		switch (*p) {
		case '!':
			if (sp->lastbcomm == NULL) {
				msgq(sp, M_ERROR,
				    "No previous command to replace \"!\".");
				return (1);
			}
			len += strlen(sp->lastbcomm);
			modified = 1;
			break;
		case '%':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERROR,
				    "No filename to substitute for %%.");
				return (1);
			}
			len += ep->nlen;
			modified = 1;
			break;
		case '#':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERROR,
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
		msgq(sp, M_ERROR, "Error: %s", strerror(errno));
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
		*p = '\0';
	} else
		memmove(com, cmdp->string, len);
		

	/* Swap commands. */
	if (sp->lastbcomm)
		free(sp->lastbcomm);
	sp->lastbcomm = com;

	AUTOWRITE(sp, ep);
	MODIFY_WARN(sp, ep);

	/* If modified, echo the new command. */
	if (modified)
		(void)fprintf(sp->stdfp, "%s\n", com);

	/*
	 * If no addresses were specified, just run the command, otherwise
	 * pipe lines from the file through the command.
	 */
	if (cmdp->addrcnt != 0) {
		if (filtercmd(sp, ep,
		    &cmdp->addr1, &cmdp->addr2, &rm, com, STANDARD))
			return (1);
		sp->lno = rm.lno;
		F_SET(sp, S_AUTOPRINT);
		return (0);
	}

	/* Save ex/vi terminal settings, and restore the original ones. */
	(void)tcgetattr(STDIN_FILENO, &savet);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &sp->gp->original_termios);

	/* Start with a new line. */
	(void)write(STDOUT_FILENO, "\n", 1);

	rval = esystem(sp, PVAL(O_SHELL), com) ? 1 : 0;

	/* Ex terminates with a bang. */
	switch(sp->flags & (S_MODE_EX | S_MODE_VI)) {
	case S_MODE_EX:
		(void)write(STDOUT_FILENO, "!\n", 2);
		break;
	case S_MODE_VI:
#define	CSTRING	"Enter any key to continue:"
		(void)write(STDOUT_FILENO, CSTRING, sizeof(CSTRING) - 1);
		break;
	default:
		abort();
	}

	/* Restore ex/vi terminal settings. */
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &savet);

	if (F_ISSET(sp, S_MODE_VI))
		(void)getkey(sp, 0);

	/* Repaint the screen. */
	F_SET(sp, S_REFRESH);

	return (rval);
}
