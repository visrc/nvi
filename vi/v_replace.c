/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 5.2 1992/04/22 08:10:28 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:28 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

MARK
v_replace(m, cnt, key)
	MARK	m;	/* first char to be replaced */
	long	cnt;	/* number of chars to replace */
	int	key;	/* what to replace them with */
{
	REG char	*text;
	REG int		i;

	SETDEFCNT(1);

	/* map ^M to '\n' */
	if (key == '\r')
	{
		key = '\n';
	}

	/* make sure the resulting line isn't too long */
	if (cnt > BLKSIZE - 2 - markidx(m))
	{
		cnt = BLKSIZE - 2 - markidx(m);
	}

	/* build a string of the desired character with the desired length */
	for (text = tmpblk.c, i = cnt; i > 0; i--)
	{
		*text++ = key;
	}
	*text = '\0';

	/* make sure cnt doesn't extend past EOL */
	pfetch(markline(m));
	key = markidx(m);
	if (key + cnt > plen)
	{
		cnt = plen - key;
	}

	/* do the replacement */
	ChangeText
	{
		change(m, m + cnt, tmpblk.c);
	}

	if (*tmpblk.c == '\n')
	{
		return (m & ~(BLKSIZE - 1)) + cnt * BLKSIZE;
	}
	else
	{
		return m + cnt - 1;
	}
}
