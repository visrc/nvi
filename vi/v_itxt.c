/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.3 1992/05/07 12:48:56 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:48:56 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "exf.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_insert --
 *	Start input mode without deleting anything.
 */
MARK *
v_insert(m, cnt, key)
	MARK	*m;	/* where to start (sort of) */
	long	cnt;	/* repeat how many times? */
	int	key;	/* what command is this for? {a,A,i,I,o,O} */
{
	static MARK rval;
	size_t len;
	int	wasdot;
	long	reps;
	int	above;	/* boolean: new line going above old line? */

	SETDEFCNT(1);

	/* tweak the insertion point, based on command key */
	above = FALSE;
	switch (key)
	{
	  case 'i':
		break;

	  case 'a':
		(void)file_line(curf, m->lno, &len);
		if (len > 0)
		{
			++m->cno;
		}
		break;

	  case 'I':
		m = m_front(m, 1L);
		break;

	  case 'A':
		(void)file_line(curf, m->lno, &len);
		m->cno = len;
		break;

	  case 'O':
		add(m, "\n", 1);
		above = TRUE;
		break;

	  case 'o':
		m->lno++;
		add(m, "\n", 1);
		break;
	}

	/* insert the same text once or more */
	for (reps = cnt, wasdot = doingdot; reps > 0; reps--, doingdot = TRUE)
	{
		m = input(m, m, WHEN_VIINP, above);
		++m->cno;
	}
	if (m->cno > 1)
	{
		--m->cno;
	}

	doingdot = wasdot;

	rval = *m;
	return (&rval);
}
