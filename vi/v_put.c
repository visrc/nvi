/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 5.3 1992/05/07 12:49:06 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:49:06 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_paste --
 *	Paste text from a cut buffer.
 */
/* ARGSUSED */
MARK *
v_paste(m, cnt, cmd)
	MARK	*m;	/* where to paste the text */
	long	cnt;	/* (ignored) */
	int	cmd;	/* either 'p' or 'P' */
{
	m = paste(m, cmd == 'p', 0);
	return m;
}
