/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_util.c,v 5.8 1992/10/29 14:45:35 bostic Exp $ (Berkeley) $Date: 1992/10/29 14:45:35 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

/*
 * v_eof --
 *	Vi end-of-file error.
 */
void
v_eof(mp)
	MARK *mp;
{
	u_long lno;

	bell();
	if (ISSET(O_VERBOSE))
		if (mp == NULL)
			msg("Already at end-of-file.");
		else {
			lno = file_lline(curf);
			if (mp->lno >= lno)
				msg("Already at end-of-file.");
			else
				msg("Movement past the end-of-file.");
		}
}

/*
 * v_eol --
 *	Vi end-of-line error.
 */
void
v_eol(mp)
	MARK *mp;	
{
	size_t len;
	u_char *p;

	bell();
	if (ISSET(O_VERBOSE))
		if (mp == NULL)
			msg("Already at end-of-line.");
		else {
			if ((p = file_gline(curf, mp->lno, &len)) == NULL) {
				GETLINE_ERR(mp->lno);
				return;
			}
			if (mp->cno == len - 1)
				msg("Already at end-of-line.");
			else
				msg("Movement past the end-of-line.");
		}
}

/*
 * v_sof --
 *	Vi start-of-file error.
 */
void
v_sof(mp)
	MARK *mp;
{
	bell();
	if (ISSET(O_VERBOSE))
		if (mp == NULL || mp->lno == 1)
			msg("Already at the beginning of the file.");
		else
			msg("Movement past the beginning of the file.");
}
