/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 5.1 1992/05/15 11:10:40 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:10:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "extern.h"

static MARK sq =		/* Initialize to the start of the file. */
	{ 1 };
static MARK marks[26] = {	/* Initialize to invalid. */
	{ OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO },
	{ OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO },
	{ OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO },
	{ OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO },
	{ OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO }, { OOBLNO },
	{ OOBLNO },
};

void
mark_def(mp)
	MARK *mp;
{
	sq = *mp;
}

int
mark_set(key, mp)
	int key;
	MARK *mp;
{
	if (key < 'a' || key > 'z') {
		msg("Invalid mark; use 'a' to 'z'.");
		return (1);
	}
	marks[key - 'a'] = *mp;
	return (0);
}

MARK *
mark_get(key)
	int key;
{
	MARK *mp;

	if (key == '\'')
		return (&sq);
	if (key < 'a' || key > 'z') {
		msg("Invalid mark; use 'a' to 'z'.");
		return (NULL);
	}
	mp = &marks[key - 'a'];
	if (mp->lno == OOBLNO) {
		bell();
		if (ISSET(O_VERBOSE))
			msg("Mark '%c not set.", key);
                return (NULL);
	}
	return (mp);
}
