/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_increment.c,v 5.1 1992/04/18 19:39:31 bostic Exp $ (Berkeley) $Date: 1992/04/18 19:39:31 $";
#endif /* not lint */

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

#ifndef NO_EXTENSIONS
MARK
v_increment(keyword, m, cnt)
	char	*keyword;
	MARK	m;
	long	cnt;
{
	static	sign;
	char	newval[12];
	long	atol();

	SETDEFCNT(1);

	/* get one more keystroke, unless doingdot */
	if (!doingdot)
	{
		sign = getkey(0);
	}

	/* adjust the number, based on that second keystroke */
	switch (sign)
	{
	  case '+':
	  case '#':
		cnt = atol(keyword) + cnt;
		break;

	  case '-':
		cnt = atol(keyword) - cnt;
		break;

	  case '=':
		break;

	  default:
		return MARK_UNSET;
	}
	sprintf(newval, "%ld", cnt);

	ChangeText
	{
		change(m, m + strlen(keyword), newval);
	}

	return m;
}
#endif
