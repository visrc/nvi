/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_bang.c,v 5.24 1993/01/31 10:27:28 bostic Exp $ (Berkeley) $Date: 1993/01/31 10:27:28 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

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
	extern struct termios original_termios;
	struct termios savet;
	static u_char *lastcom;
	register int ch, len, modified;
	register u_char *p, *t;
	int rval;
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
	if (cmdp->addrcnt != 0) {
		if (filter(&cmdp->addr1, &cmdp->addr2, com, STANDARD))
			return (1);
		FF_SET(curf, F_AUTOPRINT);
		return (0);
	}

	/* Save ex/vi terminal settings, and restore the original ones. */
	(void)tcgetattr(STDIN_FILENO, &savet);
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &original_termios);

	/* Start with a new line. */
	(void)write(STDOUT_FILENO, "\n", 1);

	rval = esystem(PVAL(O_SHELL), com) ? 1 : 0;

	/* Ex terminates with a bang. */
	switch(mode) {
	case MODE_EX:
		(void)write(STDOUT_FILENO, "!\n", 2);
		break;
	case MODE_VI:
#define	CSTRING	"Enter any key to continue:"
		(void)write(STDOUT_FILENO, CSTRING, sizeof(CSTRING) - 1);
		break;
	default:
		abort();
	}

	/* Restore ex/vi terminal settings. */
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &savet);

	if (mode == MODE_VI) {
		(void)getkey(0);

		/* Repaint the screen. */
		FF_SET(curf, F_REFRESH);
	}

	return (rval);
}
