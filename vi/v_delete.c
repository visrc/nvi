/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 5.2 1992/04/22 08:10:20 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:20 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_delete --
 *	Deletes a range of text.
 */
MARK
v_delete(m, n)
	MARK	m, n;	/* range of text to delete */
{
	/* illegal to try and delete nothing */
	if (n <= m)
	{
		return MARK_UNSET;
	}

	/* Do it */
	ChangeText
	{
		cut(m, n);
		delete(m, n);
	}
	return m;
}
