/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_zexit.c,v 10.3 1995/09/21 10:59:29 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:59:29 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "vi.h"

/*
 * v_zexit -- ZZ
 *	Save the file and exit.
 *
 * PUBLIC: int v_zexit __P((SCR *, VICMD *));
 */
int
v_zexit(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	/* Write back any modifications. */
	if (F_ISSET(sp->ep, F_MODIFIED) &&
	    file_write(sp, NULL, NULL, NULL, FS_ALL))
		return (1);

	/* Check to make sure it's not a temporary file. */
	if (file_m3(sp, 0))
		return (1);

	/* Check for more files to edit. */
	if (ex_ncheck(sp, 0))
		return (1);

	F_SET(sp, S_EXIT);
	return (0);
}
