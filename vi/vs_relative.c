/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_relative.c,v 8.8 1994/03/08 19:40:41 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:40:41 $";
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
#include "svi_screen.h"

/*
 * svi_column --
 *	Return the logical column of the cursor.
 */
int
svi_column(sp, ep, cp)
	SCR *sp;
	EXF *ep;
	size_t *cp;
{
	size_t col;

	col = SVP(sp)->sc_col;
	if (O_ISSET(sp, O_NUMBER))
		col -= O_NUMBER_LENGTH;
	*cp = col;
	return (0);
}

/*
 * svi_relative --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position.  If it's not easy, uses the underlying routine that
 *	really figures it out.  It's broken into two parts because the
 *	svi_lrelative routine handles "logical" offsets, which nobody
 *	but the screen routines understand.
 */
size_t
svi_relative(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	size_t cno;

	/* First non-blank character. */
	if (sp->rcmflags == RCM_FNB) {
		cno = 0;
		(void)nonblank(sp, ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (sp->rcmflags != RCM_LAST && sp->rcm == 0)
		return (0);

	return (svi_lrelative(sp, ep, lno, 1));
}

/*
 * svi_lrelative --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position.  The offset is for the commands that move logical
 *	distances, i.e. if it's a logical scroll the closest physical
 *	distance is based on the logical line, not the physical line.
 */
size_t
svi_lrelative(sp, ep, lno, off)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t off;
{
	CHNAME const *cname;
	size_t len, llen, scno;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	if ((lp = file_gline(sp, ep, lno, &len)) == NULL)
		return (0);

	/* Empty lines are easy. */
	if (len == 0)
		return (0);

	/* Last character is easy, and common. */
	if (sp->rcmflags == RCM_LAST)
		return (len - 1);

	/* Discard logical lines. */
	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);
	for (scno = 0, p = lp, llen = len; --off;) {
		for (; len && scno < sp->cols; --len)
			SCNO_INCREMENT;
		if (len == 0)
			return (llen - 1);
		scno -= sp->cols;
	}

	/* Step through the line until reach the right character. */
	while (len--) {
		SCNO_INCREMENT;
		if (scno >= sp->rcm) {
			/* Get the offset of this character. */
			len = p - lp;

			/*
			 * May be the next character, not this one,
			 * so check to see if we've gone too far.
			 */
			if (scno == sp->rcm)
				return (len < llen - 1 ? len : llen - 1);
			/* It's this character. */
			return (len - 1);
		}
	}
	/* No such character; return start of last character. */
	return (llen - 1);
}

/*
 * svi_chposition --
 *	Return the physical column from the line that will display a
 *	character closest to the specified column.
 */
size_t
svi_chposition(sp, ep, lno, cno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t cno;
{
	CHNAME const *cname;
	size_t len, llen, scno;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	if ((lp = file_gline(sp, ep, lno, &llen)) == NULL)
		return (0);

	/* Empty lines are easy. */
	if (llen == 0)
		return (0);

	/* Step through the line until reach the right character. */
	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);
	for (scno = 0, len = llen, p = lp; len--;) {
		SCNO_INCREMENT;
		if (scno >= cno) {
			/* Get the offset of this character. */
			len = p - lp;

			/*
			 * May be the next character, not this one,
			 * so check to see if we've gone too far.
			 */
			if (scno == cno)
				return (len < llen - 1 ? len : llen - 1);
			/* It's this character. */
			return (len - 1);
		}
	}
	/* No such character; return start of last character. */
	return (llen - 1);
}
