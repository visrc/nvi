/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_yank.c,v 5.1 1992/04/18 19:39:56 bostic Exp $ (Berkeley) $Date: 1992/04/18 19:39:56 $";
#endif /* not lint */

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_yank --
 *	Yank text into a cut buffer.
 */
MARK v_yank(m, n)
	MARK	m, n;	/* range of text to yank */
{
	cut(m, n);
	return m;
}


