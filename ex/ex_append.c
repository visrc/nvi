/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 5.15 1992/10/10 13:57:45 bostic Exp $ (Berkeley) $Date: 1992/10/10 13:57:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <termios.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"
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
	size_t len;
	int set;
	u_char *p;

	/* The ! flag turns off autoindent for change and append. */
	if (cmdp->flags & E_FORCE) {
		set = ISSET(O_AUTOINDENT);
		UNSET(O_AUTOINDENT);
	} else
		set = 0;

	/* If we're doing a change, delete the old version. */
	if (cmd == CHANGE)
		ex_delete(cmdp);

	/*
	 * New lines start at the specified line for changes,
	 * after it for appends.
	 */
	m = cmdp->addr1;
	if (cmd == APPEND)
		++m.lno;

	/* Insert lines until no more lines, or "." line. */
	EX_PRSTART(0);
	for (; !gb(0, &p, &len, GB_NL|GB_NLECHO) && p != NULL; ++m.lno) {
		if (p[0] == '.' && p[1] == '\0')
			break;
		add(&m, p, len);
	}

	autoprint = 1;

	if (set)
		SET(O_AUTOINDENT);
}
