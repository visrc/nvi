/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_relative.c,v 5.1 1993/04/17 15:04:08 bostic Exp $ (Berkeley) $Date: 1993/04/17 15:04:08 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "svi_screen.h"

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
		(void)nonblank(sp, ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (sp->rcmflags != RCM_LAST && sp->sc_col == 0)
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
	CHNAME *cname;
	size_t len, llen, scno;
	int ch;
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

	/* Set scno to the right initial value. */
	scno = O_ISSET(sp, O_NUMBER) ? O_NUMBER_LENGTH : 0;

	/* Discard logical lines. */
	cname = sp->cname;
	for (p = lp, llen = len; --off;) {
		for (; len && scno < sp->cols; --len) {
			ch = *(u_char *)p++;
			if (ch == '\t' && !O_ISSET(sp, O_LIST))
				scno += TAB_OFF(sp, scno);
			else
				scno += cname[ch].len;
		}
		if (len == 0)
			return (llen - 1);
		scno -= sp->cols;
	}

	/* Step through the line until reach the right character. */
	while (len--) {
		ch = *(u_char *)p++;
		if (ch == '\t' && !O_ISSET(sp, O_LIST))
			scno += TAB_OFF(sp, scno);
		else
			scno += cname[ch].len;

		if (scno >= sp->rcm) {
			/* Get the offset of this character. */
			len = p - lp;

			/*
			 * It's the next character, not this one, so check
			 * to see if we've gone too far.
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
