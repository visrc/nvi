/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: mark.c,v 5.2 1992/05/17 15:32:49 bostic Exp $ (Berkeley) $Date: 1992/05/17 15:32:49 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "extern.h"

static MARK marks[UCHAR_MAX + 1];

int
mark_set(key, mp)
	int key;
	MARK *mp;
{
	if (key > UCHAR_MAX) {
		bell();
		msg("Invalid mark name.");
		return (1);
	}
	marks[key] = *mp;
	return (0);
}

MARK *
mark_get(key)
	int key;
{
	MARK *mp;

	if (key > UCHAR_MAX) {
		bell();
		msg("Invalid mark name.");
		return (NULL);
	}
	mp = &marks[key];
	if (mp->lno == OOBLNO) {
		bell();
		msg("Mark '%c not set.", key);
                return (NULL);
	}
	return (mp);
}
