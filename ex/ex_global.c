/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 5.4 1992/04/15 09:13:36 bostic Exp $ (Berkeley) $Date: 1992/04/15 09:13:36 $";
#endif /* not lint */

#include <sys/types.h>
#include <regexp.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {GLOBAL, VGLOBAL};
static void global __P((CMDARG *, enum which));

int
ex_vglobal(cmdp)
	CMDARG *cmdp;
{
	global(cmdp, VGLOBAL);
	return (0);
}

int
ex_global(cmdp)
	CMDARG *cmdp;
{
	global(cmdp, VGLOBAL);
	return (0);
}

static void
global(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	char	*cmdptr;	/* the command from the command line */
	char	cmdln[100];	/* copy of the command from the command line */
	char	*line;		/* a line from the file */
	long	l;		/* used as a counter to move through lines */
	long	lqty;		/* quantity of lines to be scanned */
	long	nchanged;	/* number of lines changed */
	int isv;
	regexp	*re;		/* the compiled search expression */
	char *extra;

/*
 * XXX
 * If no addresses, do all addresses.
 */
	extra = cmdp->argv[0];

	/* can't nest global commands */
	if (doingglobal)
	{
		msg("Can't nest global commands.");
		rptlines = -1L;
		return;
	}

	/* XXX ??? ":g! ..." is the same as ":v ..." */
	if (cmdp->flags & E_FORCE)
		cmd = VGLOBAL;

	/* make sure we got a search pattern */
	if (*extra != '/' && *extra != '?')
	{
		msg("Usage: %c /regular expression/ command", cmd == VGLOBAL ? 'v' : 'g');
		return;
	}

	/* parse & compile the search pattern */
	cmdptr = parseptrn(extra);
	if (!extra[1])
	{
		msg("Can't use empty regular expression with '%c' command", cmd == VGLOBAL ? 'v' : 'g');
		return;
	}
	re = regcomp(extra + 1);
	if (!re)
	{
		/* regcomp found & described an error */
		return;
	}

	/* for each line in the range */
	doingglobal = TRUE;
	isv = cmd == VGLOBAL;
	ChangeText
	{
		/* NOTE: we have to go through the lines in a forward order,
		 * otherwise "g/re/p" would look funny.  *BUT* for "g/re/d"
		 * to work, simply adding 1 to the line# on each loop won't
		 * work.  The solution: count lines relative to the end of
		 * the file.  Think about it.
		 */
		for (l = nlines - markline(cmdp->addr1),
			lqty = markline(cmdp->addr2) - markline(cmdp->addr1) + 1L,
			nchanged = 0L;
		     lqty > 0 && nlines - l >= 0 && nchanged >= 0L;
		     l--, lqty--)
		{
			/* fetch the line */
			line = fetchline(nlines - l, NULL);

			/* if it contains the search pattern... */
			if ((!regexec(re, line, 1)) == isv)
			{
				/* move the cursor to that line */
				cursor = MARK_AT_LINE(nlines - l);

				/* do the ex command (without mucking up
				 * the original copy of the command line)
				 */
				strcpy(cmdln, cmdptr);
				rptlines = 0L;
				ex_cmd(cmdln);
				nchanged += rptlines;
			}
		}
	}
	doingglobal = FALSE;

	/* free the regexp */
	free(re);

	/* Reporting...*/
	rptlines = nchanged;
}
