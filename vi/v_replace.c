/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 5.4 1992/05/07 12:49:13 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:13 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "exf.h"
#include "vcmd.h"
#include "extern.h"

MARK *
v_replace(m, cnt, key)
	MARK	*m;	/* first char to be replaced */
	long	cnt;	/* number of chars to replace */
	int	key;	/* what to replace them with */
{
	static MARK rval;
	REG char	*text;
	REG int		i;
	size_t len, ilen;
	char lbuf[1024];

	SETDEFCNT(1);

	/* map ^M to '\n' */
	if (key == '\r')
	{
		key = '\n';
	}

	/* build a string of the desired character with the desired length */
	for (text = lbuf, i = cnt, ilen = 0; i > 0; i--)
	{
		*text++ = key;
		++ilen;
	}
	*text = '\0';

	/* make sure cnt doesn't extend past EOL */
	(void)file_line(curf, m->lno, &len);
	key = m->cno;
	if (key + cnt > len)
	{
		cnt = len - key;
	}

	/* do the replacement */
	rval = *m;
	rval.cno += cnt;
	change(m, &rval, lbuf, ilen);

	if (*lbuf == '\n')
	{
		++m->lno;
		m->cno = 1;
	}
	else
	{
		m->cno += cnt - 1;
	}
}
