/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: vs_relative.c,v 8.10 1994/03/10 11:21:57 bostic Exp $ (Berkeley) $Date: 1994/03/10 11:21:57 $";
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
 * svi_opt_screens --
 *	Return the screen columns necessary to display the line, or
 *	if specified, the physical character column within the line,
 *	including space required for the O_NUMBER and O_LIST options.
 */
size_t
svi_opt_screens(sp, ep, lno, cnop)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t *cnop;
{
	size_t cols, screens;

	/*
	 * Check for a cached value.  We maintain a cache because, if the
	 * line is large, this routine gets called repeatedly.  One other
	 * hack, lots of time the cursor is on column one, which is an easy
	 * one.
	 */
	if (cnop == NULL) {
		if (SVP(sp)->ss_lno == lno)
			return (SVP(sp)->ss_screens);
	} else if (*cnop == 0)
		return (1);

	/* Figure out how many columns the line/column needs. */
	cols = svi_screens(sp, ep, NULL, 0, lno, cnop);

	/* Leading number if O_NUMBER option set. */
	if (O_ISSET(sp, O_NUMBER))
		cols += O_NUMBER_LENGTH;

	/* Trailing '$' if O_LIST option set. */
	if (O_ISSET(sp, O_LIST) && cnop == NULL)
		cols += sp->gp->cname['$'].len;

	screens = (cols / sp->cols + (cols % sp->cols ? 1 : 0));
	if (screens == 0)
		screens = 1;

	/* Cache the value. */
	if (cnop == NULL) {
		SVP(sp)->ss_lno = lno;
		SVP(sp)->ss_screens = screens;
	}
	return (screens);
}

/*
 * svi_screens --
 *	Return the screen columns necessary to display the line, or,
 *	if specified, the physical character column within the line.
 */
size_t
svi_screens(sp, ep, lp, llen, lno, cnop)
	SCR *sp;
	EXF *ep;
	char *lp;
	size_t llen;
	recno_t lno;
	size_t *cnop;
{
	CHNAME const *cname;
	size_t chlen, cno, len, scno, tab_off;
	int ch, listset;
	char *p;

	/* Need the line to go any further. */
	if (lp == NULL)
		lp = file_gline(sp, ep, lno, &llen);

	/* Missing or empty lines are easy. */
	if (lp == NULL || llen == 0)
		return (0);

	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);

#define	SET_CHLEN {							\
	chlen = (ch = *(u_char *)p++) == '\t' &&			\
	    !listset ? TAB_OFF(sp, tab_off) : cname[ch].len;		\
}
#define	TAB_RESET {							\
	/*								\
	 * If past the end of the screen, and the character was a tab,	\
	 * reset the screen column to 0.  Otherwise, display the rest	\
	 * of the character on the next line.				\
	 */								\
	if ((tab_off += chlen) >= sp->cols)				\
		if (ch == '\t') {					\
			tab_off = 0;					\
			scno -= scno % sp->cols;			\
		} else							\
			tab_off -= sp->cols;				\
}
	p = lp;
	len = llen;
	scno = tab_off = 0;
	if (cnop == NULL)
		while (len--) {
			SET_CHLEN;
			scno += chlen;
			TAB_RESET;
		}
	else
		for (cno = *cnop; len-- && cno--;) {
			SET_CHLEN;
			scno += chlen;
			TAB_RESET;
		}
	return (scno);
}

/*
 * svi_rcm_public --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position, taking into account the screen offset.
 *
 *	The extra interface is because it's called by vi, which doesn't
 *	have a handle on the SMAP structure.
 */
size_t
svi_rcm_public(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	return (svi_rcm_private(sp, ep, lno, HMAP->off));
}

/*
 * svi_rcm_private --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position (which is stored as a screen column).
 *
 *	The offset is for the commands that move logical distances, i.e.
 *	if it's a logical scroll the closest physical distance is based
 *	on the logical line, not the physical line.
 */
size_t
svi_rcm_private(sp, ep, lno, off)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t off;
{
	size_t cno, len;

	/* First non-blank character. */
	if (sp->rcmflags == RCM_FNB) {
		cno = 0;
		(void)nonblank(sp, ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (sp->rcmflags != RCM_LAST && off == 1 && sp->rcm == 0)
		return (0);

	/* Last character is easy, and common. */
	if (sp->rcmflags == RCM_LAST)
		return (file_gline(sp,
		    ep, lno, &len) == NULL || len == 0 ? 0 : len - 1);

	/* Get svi_cm_private() to do the hard work. */
	return (svi_cm_private(sp, ep, lno, 1, sp->rcm));
}

/*
 * svi_cm_public --
 *	Return the physical column from the line that will display a
 *	character closest to the specified screen column.
 *
 *	The extra interface is because it's called by vi, which doesn't
 *	have a handle on the SMAP structure.
 */
size_t
svi_cm_public(sp, ep, lno, cno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t cno;
{
	return (svi_cm_private(sp, ep, lno, HMAP->off, cno));
}

/*
 * svi_cm_private --
 *	Return the physical column from the line that will display a
 *	character closest to the specified screen column, taking into
 *	account the screen offset. 
 *
 *	The offset is for the commands that move logical distances, i.e.
 *	if it's a logical scroll the closest physical distance is based
 *	on the logical line, not the physical line.
 */
size_t
svi_cm_private(sp, ep, lno, off, cno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t off, cno;
{
	CHNAME const *cname;
	size_t chlen, len, llen, scno, tab_off;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	lp = file_gline(sp, ep, lno, &llen);

	/* Missing or empty lines are easy. */
	if (lp == NULL || llen == 0)
		return (0);

	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);

	/* Discard screen (logical) lines. */
	for (scno = 0, p = lp, len = llen; --off;) {
		while (len-- && scno < sp->cols)
			scno += (ch = *(u_char *)p++) == '\t' &&
			    !listset ? TAB_OFF(sp, scno) : cname[ch].len;
			
		/*
		 * If reached the end of the physical line, return
		 * the last physical character in the line.
		 */
		if (len == 0)
			return (llen - 1);

		/*
		 * If the character was a tab, reset the screen column to 0.
		 * Otherwise, the rest of the character is displayed on the
		 * next line.
		 */
		if (ch == '\t')
			scno = 0;
		else
			scno -= sp->cols;
	}

	/* Step through the line until reach the right character or EOL. */
	for (tab_off = scno; len--;) {
		SET_CHLEN;

		/*
		 * If we've reached the specific character, there are three
		 * cases.
		 *
		 * 1: scno == cno, i.e. the current character ends at the
		 *    screen character we care about.
		 *	a: off < llen - 1, i.e. not the last character in
		 *	   the line, return the offset of the next character.
		 *	b: else return the offset of the last character.
		 * 2: scno != cno, i.e. this character overruns the character
		 *    we care about, return the offset of this character.
		 */
		if ((scno += chlen) >= cno) {
			off = p - lp;
			return (scno == cno ?
			    (off < llen - 1 ? off : llen - 1) : off - 1);
		}

		TAB_RESET;
	}

	/* No such character; return the start of the last character. */
	return (llen - 1);
}
