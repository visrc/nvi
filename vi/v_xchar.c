/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_xchar.c,v 5.2 1992/04/22 08:10:38 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:38 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_xchar --
 *	Deletes the character(s) that the cursor is on.
 */
MARK
v_xchar(m, cnt, cmd)
	MARK	m;	/* where to start deletions */
	long	cnt;	/* number of chars to delete */
	int	cmd;	/* either 'x' or 'X' */
{
	SETDEFCNT(1);

	/* for 'X', adjust so chars are deleted *BEFORE* cursor */
	if (cmd == 'X')
	{
		if (markidx(m) < cnt)
			return MARK_UNSET;
		m -= cnt;
	}

	/* make sure we don't try to delete more thars than there are */
	pfetch(markline(m));
	if (markidx(m + cnt) > plen)
	{
		cnt = plen - markidx(m);
	}
	if (cnt == 0L)
	{
		return MARK_UNSET;
	}

	/* do it */
	ChangeText
	{
		cut(m, m + cnt);
		delete(m, m + cnt);
	}
	return m;
}
