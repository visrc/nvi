/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_delete.c,v 5.3 1992/05/07 12:48:43 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:48:43 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_delete --
 *	Deletes a range of text.
 */
MARK *
v_delete(m, n)
	MARK	*m, *n;	/* range of text to delete */
{
	static MARK rval;
	/* illegal to try and delete nothing */
	if (n->lno <= m->lno)
	{
		return NULL;
	}

	/* Do it */
	cut(m, n);
	delete(m, n);
	rval = *m;
	return (&rval);
}
