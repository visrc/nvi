/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 5.10 1992/05/07 12:49:15 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:15 $";
#endif /* not lint */

#include <sys/types.h>
#include <curses.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/* ARGSUSED */
MARK *
v_wsearch(word, m, cnt)
	char	*word;	/* the word to search for */
	MARK	*m;	/* the starting point */
	int	cnt;	/* ignored */
{
	char buf[1024];

	/* Build a "word" pattern. */
	(void)snprintf(buf, sizeof(buf), "\\<%s\\>", word);

	/* Show the searched-for word on the bottom line. */
	move(LINES - 1, 0);
	addstr(buf);
	clrtoeol();
	refresh();

	return (f_search(m, buf, NULL, 1));
}

MARK *
v_nsearch(m)
	MARK *m;
{
	switch(searchdir) {
	case BACKWARD:
		return (b_search(m, NULL, NULL, 1));
		/* NOTREACHED */
	case FORWARD:
		return (f_search(m, NULL, NULL, 1));
		/* NOTREACHED */
	}
}

MARK *
v_Nsearch(m)
	MARK *m;
{
	switch(searchdir) {
	case BACKWARD:
		m = f_search(m, NULL, NULL, 1);
		searchdir = BACKWARD;
		break;
	case FORWARD:
		m = b_search(m, NULL, NULL, 1);
		searchdir = FORWARD;
		break;
	}
	return (m);
}
