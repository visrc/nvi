/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 5.17 1992/10/30 23:12:48 bostic Exp $ (Berkeley) $Date: 1992/10/30 23:12:48 $";
#endif /* not lint */

#include <sys/types.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "term.h"
#include "vcmd.h"
#include "extern.h"

static int getptrn __P((int, u_char **));

/*
 * v_searchn -- n
 *	Repeat last search.
 */
int
v_searchn(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;

	switch(searchdir) {
	case BACKWARD:
		if ((m = b_search(fm, NULL, NULL, 0)) == NULL)
			return (1);
		*rp = *m;
		break;
	case FORWARD:
		if ((m = f_search(fm, NULL, NULL, 0)) == NULL)
			return (1);
		*rp = *m;
		break;
	case NOTSET:
		msg("No previous search pattern.");
		*rp = *fm;
		break;
	}
	return (0);
}

/*
 * v_searchN -- N
 *	Reverse last search.
 */
int
v_searchN(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;

	switch(searchdir) {
	case BACKWARD:
		if ((m = f_search(fm, NULL, NULL, 0)) == NULL)
			return (1);
		*rp = *m;
		break;
	case FORWARD:
		if ((m = b_search(fm, NULL, NULL, 0)) == NULL)
			return (1);
		*rp = *m;
		break;
	case NOTSET:
		msg("No previous search pattern.");
		*rp = *fm;
		break;
	}
	return (0);
}

/*
 * v_searchw -- [count]^A
 *	Search for the cursor word.
 */
int
v_searchw(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;
	char buf[1024];

	/* Build a "word" pattern. */
	if (snprintf(buf, sizeof(buf), "\\<%s\\>", vp->keyword) >=
	    sizeof(buf)) {
		msg("Word too long to search for.");
		return (1);
	}

	/* Show the searched-for word on the bottom line. */
	msg("%s", vp->keyword);
	msg_vflush(curf);

	if ((mp = f_search(fm, (u_char *)buf, NULL, 1)) == NULL)
		return (1);
	*rp = *mp;
	return (0);
}

/*
 * v_searchb -- [count]?RE
 *	Search backward.
 */
v_searchb(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn('?', &ptrn))
		return (1);
	if (ptrn == NULL)
		return (0);
	if ((m = b_search(fm, ptrn, NULL, 1)) == NULL)
		return (1);
	*rp = *m;
	return (0);
}

/*
 * v_searchf -- [count]/RE
 *	Search forward.
 */
v_searchf(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn('/', &ptrn))
		return (1);
	if (ptrn == NULL)
		return (0);
	if ((m = f_search(fm, ptrn, NULL, 1)) == NULL)
		return (1);
	*rp = *m;
	return (0);
}

/*
 * getptrn --
 *	Get the search pattern.
 */
static int
getptrn(prompt, storep)
	int prompt;
	u_char **storep;
{
	v_startex();
	if (gb(prompt, storep, NULL, GB_BS|GB_ESC))
		return (1);
	v_leaveex();
	return (0);
}
