/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_mark.c,v 5.1 1992/04/18 19:39:35 bostic Exp $ (Berkeley) $Date: 1992/04/18 19:39:35 $";
#endif /* not lint */

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_mark --
 *	Define a mark.
 */
/* ARGSUSED */
MARK
v_mark(m, count, key)
	MARK	m;	/* where the mark will be */
	long	count;	/* (ignored) */
	int	key;	/* the ASCII label of the mark */
{
	if (key < 'a' || key > 'z')
	{
		msg("Marks must be from a to z");
	}
	else
	{
		mark[key - 'a'] = m;
	}
	return m;
}
