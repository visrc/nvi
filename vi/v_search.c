/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 5.8 1992/04/22 09:27:37 bostic Exp $ (Berkeley) $Date: 1992/04/22 09:27:37 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "curses.h"
#include "vcmd.h"
#include "extern.h"

/* ARGSUSED */
MARK
v_wsearch(word, m, cnt)
	char	*word;	/* the word to search for */
	MARK	m;	/* the starting point */
	int	cnt;	/* ignored */
{
	char buf[1024];

	/* Build a "word" pattern. */
	(void)snprintf(buf, sizeof(buf), "\\<%s\\>", word);

	/* Show the searched-for word on the bottom line. */
	move(LINES - 1, 0);
	qaddstr(buf);
	clrtoeol();
	refresh();

	return (f_search(m, buf));
}

MARK
v_nsearch(m)
	MARK m;
{
	switch(searchdir) {
	case BACKWARD:
		return (b_search(m, NULL));
		/* NOTREACHED */
	case FORWARD:
		return (f_search(m, NULL));
		/* NOTREACHED */
	}
}

MARK
v_Nsearch(m)
	MARK m;
{
	switch(searchdir) {
	case BACKWARD:
		m = f_search(m, NULL);
		searchdir = BACKWARD;
		break;
	case FORWARD:
		m = b_search(m, NULL);
		searchdir = FORWARD;
		break;
	}
	return (m);
}
