/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_xchar.c,v 5.4 1992/05/15 11:14:26 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:26 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_xchar --
 *	Deletes the character(s) that the cursor is on.
 */
MARK *
v_xchar(m, cnt, cmd)
	MARK	*m;	/* where to start deletions */
	long	cnt;	/* number of chars to delete */
	int	cmd;	/* either 'x' or 'X' */
{
	MARK t;
	size_t len;

	SETDEFCNT(1);

	/* for 'X', adjust so chars are deleted *BEFORE* cursor */
	if (cmd == 'X')
	{
		if (m->cno < cnt)
			return NULL;
		m->cno -= cnt;
	}

	/* make sure we don't try to delete more thars than there are */
	(void)file_gline(curf, m->lno, &len);
	if (m->cno + cnt > len)
	{
		cnt = len - m->cno;
	}
	if (cnt == 1)
	{
		return NULL;
	}

	/* do it */
	t.lno = m->lno;
	t.cno = m->cno + cnt;
	cut(m, &t);
	delete(m, &t);
	return m;
}
