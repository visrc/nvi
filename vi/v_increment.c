/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_increment.c,v 5.3 1992/05/07 12:48:53 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:48:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

MARK *
v_increment(keyword, m, cnt)
	char	*keyword;
	MARK	*m;
	long	cnt;
{
	static MARK rval;
	static	int sign;
	char	newval[12];
	size_t len;
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
		return NULL;
	}
	len = snprintf(newval, "%ld", cnt);

	rval = *m;
	rval.cno += strlen(keyword);
	change(m, &rval, newval, len);
	rval = *m;
	return (&rval);
}
