/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 5.6 1992/04/18 09:54:05 bostic Exp $ (Berkeley) $Date: 1992/04/18 09:54:05 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "curses.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

enum which {APPEND, CHANGE};

static void ca __P((CMDARG *, enum which));

/*
 * ex_append (:address append)
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(cmdp)
	CMDARG *cmdp;
{
	ca(cmdp, APPEND);
	return (0);
}

int
ex_change(cmdp)
	CMDARG *cmdp;
{
	ca(cmdp, CHANGE);
	return (0);
}

static void
ca(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	long l;
	int set;

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
		for (; vgets('\0', tmpblk.c, BLKSIZE) >= 0; ++l) {
			addch('\n');
			if (!strcmp(tmpblk.c, "."))
				break;

			(void)strcat(tmpblk.c, "\n");
			add(MARK_AT_LINE(l), tmpblk.c);
		}
	}

	/* This can be called from vi mode. */
	redraw(MARK_UNSET, FALSE);

	/* Turn on autoprint. */
	autoprint = 1;

	if (set)
		SET(O_AUTOINDENT);
}
