/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 5.12 1992/05/04 11:51:33 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:51:33 $";
#endif /* not lint */

#include <sys/types.h>
#include <termios.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "tty.h"
#include "extern.h"

enum which {APPEND, CHANGE};

static void ca __P((EXCMDARG *, enum which));

/*
 * ex_append -- :address append[!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(cmdp)
	EXCMDARG *cmdp;
{
	ca(cmdp, APPEND);
	return (0);
}

/*
 * ex_change -- :range change[!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(cmdp)
	EXCMDARG *cmdp;
{
	ca(cmdp, CHANGE);
	return (0);
}

static void
ca(cmdp, cmd)
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	long l;
	int set;
	char *p;

	/* The ! flag turns off autoindent for the command. */
	if (cmdp->flags & E_FORCE) {
		set = ISSET(O_AUTOINDENT);
		UNSET(O_AUTOINDENT);
	} else
		set = 0;

	m = cmdp->addr1;
	ChangeText
	{
		/* If we're doing a change, delete the old version. */
		if (cmd == CHANGE)
			ex_delete(cmdp);

		/*
		 * New lines start at the specified line for changes,
		 * after it for appends.
		 */
		l = markline(m);
		if (cmd == APPEND)
 			++l;

		/* Insert lines until no more lines, or "." line. */
		EX_PRSTART(0);
		for (; (p = gb(0, NULL, GB_NL|GB_NLECHO)) != NULL; ++l) {
			if (p[0] == '.' && p[1] == '\0')
				break;
			add(MARK_AT_LINE(l), p);
		}
	}

	autoprint = 1;

	if (set)
		SET(O_AUTOINDENT);
}
