/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_put.c,v 5.2 1992/04/22 08:10:26 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:26 $";
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
MARK
v_paste(m, cnt, cmd)
	MARK	m;	/* where to paste the text */
	long	cnt;	/* (ignored) */
	int	cmd;	/* either 'p' or 'P' */
{
	ChangeText
	{
		m = paste(m, cmd == 'p', FALSE);
	}
	return m;
}
