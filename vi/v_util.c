/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.29 1993/03/28 19:05:52 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:05:52 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_eof --
 *	Vi end-of-file error.
 */
void
v_eof(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;
{
	u_long lno;

	if (mp == NULL)
		msgq(sp, M_BELL, "Already at end-of-file.");
	else {
		lno = file_lline(sp, ep);
		if (mp->lno >= lno)
			msgq(sp, M_BELL, "Already at end-of-file.");
		else
			msgq(sp, M_BELL,
			    "Movement past the end-of-file.");
	}
}

/*
 * v_eol --
 *	Vi end-of-line error.
 */
void
v_eol(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;	
{
	size_t len;

	if (mp == NULL)
		msgq(sp, M_BELL, "Already at end-of-line.");
	else {
		if (file_gline(sp, ep, mp->lno, &len) == NULL) {
			GETLINE_ERR(sp, mp->lno);
			return;
		}
		if (mp->cno == len - 1)
			msgq(sp, M_BELL, "Already at end-of-line.");
		else
			msgq(sp, M_BELL, "Movement past the end-of-line.");
	}
}

/*
 * v_sof --
 *	Vi start-of-file error.
 */
void
v_sof(sp, mp)
	SCR *sp;
	MARK *mp;
{
	if (mp == NULL || mp->lno == 1)
		msgq(sp, M_BELL, "Already at the beginning of the file.");
	else
		msgq(sp, M_BELL, "Movement past the beginning of the file.");
}
