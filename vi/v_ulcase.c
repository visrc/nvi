/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ulcase.c,v 5.3 1992/04/22 08:10:36 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:36 $";
#endif /* not lint */

#include <sys/types.h>
#include <ctype.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_ulcase -- ~
 *	This function toggles upper & lower case letters.
 */
MARK
v_ulcase(m, cnt)
	MARK m;			/* Where to make the change. */
	register long cnt;	/* Number of chars to flip. */
{
	register char ch, *from, *to;
	long scnt;
	int madechange;

	SETDEFCNT(1);

	/* Fetch the current version of the line. */
	pfetch(markline(m));

	scnt = cnt;
	madechange = 0;
	for (from = &ptext[markidx(m)], to = tmpblk.c; cnt--; to)  {
		if ((ch = *from++) == '\0')
			break;
		if (isupper(ch)) {
			*to++ = tolower(ch);
			madechange = 1;
		} else if (islower(ch)) {
			*to++ = toupper(ch);
			madechange = 1;
		} else
			*to++ = ch;
	}

	if (madechange)
		ChangeText {
			tmpblk.c[scnt] = '\0';
			change(m, m + scnt, tmpblk.c);
		}
	return (m + scnt);
}
