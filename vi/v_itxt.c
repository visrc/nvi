/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_itxt.c,v 5.2 1992/04/22 08:10:23 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:23 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_insert --
 *	Start input mode without deleting anything.
 */
MARK
v_insert(m, cnt, key)
	MARK	m;	/* where to start (sort of) */
	long	cnt;	/* repeat how many times? */
	int	key;	/* what command is this for? {a,A,i,I,o,O} */
{
	int	wasdot;
	long	reps;
	int	above;	/* boolean: new line going above old line? */

	SETDEFCNT(1);

	ChangeText
	{
		/* tweak the insertion point, based on command key */
		above = FALSE;
		switch (key)
		{
		  case 'i':
			break;

		  case 'a':
			pfetch(markline(m));
			if (plen > 0)
			{
				m++;
			}
			break;

		  case 'I':
			m = m_front(m, 1L);
			break;

		  case 'A':
			pfetch(markline(m));
			m = (m & ~(BLKSIZE - 1)) + plen;
			break;

		  case 'O':
			m &= ~(BLKSIZE - 1);
			add(m, "\n");
			above = TRUE;
			break;

		  case 'o':
			m = (m & ~(BLKSIZE - 1)) + BLKSIZE;
			add(m, "\n");
			break;
		}

		/* insert the same text once or more */
		for (reps = cnt, wasdot = doingdot; reps > 0; reps--, doingdot = TRUE)
		{
			m = input(m, m, WHEN_VIINP, above) + 1;
		}
		if (markidx(m) > 0)
		{
			m--;
		}

		doingdot = wasdot;
	}

	return m;
}
