/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_zexit.c,v 8.10 1994/08/04 14:13:39 bostic Exp $ (Berkeley) $Date: 1994/08/04 14:13:39 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * v_zexit -- ZZ
 *	Save the file and exit.
 */
int
v_zexit(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/* Check to make sure it's not a temporary file. */
	if (file_m3(sp, ep, 0))
		return (1);

	/* Write back any modifications. */
	if (F_ISSET(ep, F_MODIFIED) &&
	    file_write(sp, ep, NULL, NULL, NULL, FS_ALL))
		return (1);

	/*
	 * !!!
	 * Historic practice: quit! or two quit's done in succession
	 * (where ZZ counts as a quit) didn't check for other files.
	 */
	if (sp->ccnt != sp->q_ccnt + 1 &&
	    sp->cargv != NULL && sp->cargv[1] != NULL) {
		sp->q_ccnt = sp->ccnt;
		msgq(sp, M_ERR,
	    "More files to edit; use \":n[ext]\" to go to the next file");
		return (1);
	}

	F_SET(sp, S_EXIT);
	return (0);
}
