/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 5.20 1992/11/07 19:01:00 bostic Exp $ (Berkeley) $Date: 1992/11/07 19:01:00 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "vcmd.h"
#include "search.h"
#include "term.h"
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
		if ((m = b_search(curf,
		    fm, NULL, NULL, SEARCH_MSG | SEARCH_PARSE)) == NULL)
			return (1);
		break;
	case FORWARD:
		if ((m = f_search(curf,
		    fm, NULL, NULL, SEARCH_MSG | SEARCH_PARSE)) == NULL)
			return (1);
		break;
	default:
	case NOTSET:
		msg("No previous search pattern.");
		return (1);
	}
	*rp = *m;
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
		if ((m = f_search(curf,
		    fm, NULL, NULL, SEARCH_MSG | SEARCH_PARSE)) == NULL)
			return (1);
		break;
	case FORWARD:
		if ((m = b_search(curf,
		    fm, NULL, NULL, SEARCH_MSG | SEARCH_PARSE)) == NULL)
			return (1);
		break;
	default:
	case NOTSET:
		msg("No previous search pattern.");
		return (1);
	}
	*rp = *m;
	return (0);
}

/*
 * v_searchw -- [count]^A
 *	Search for the word under the cursor.
 */
int
v_searchw(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;
	static char *wbuf;
	static size_t wbuflen;

#define	WORDFORMAT	"[^[:alnum:]]%s[^[:alnum:]]"

	if (vp->kbuflen > wbuflen + sizeof(WORDFORMAT)) {
		wbuflen = MAX(vp->kbuflen + sizeof(WORDFORMAT), 256);
		if ((wbuf = realloc(wbuf, wbuflen)) == NULL) {
			wbuf = NULL;
			wbuflen = 0;
			msg("Word too long to search for.");
			return (1);
		}
	}
	(void)snprintf(wbuf, wbuflen, WORDFORMAT, vp->keyword);
		
	if ((mp = f_search(curf, fm, (u_char *)wbuf, NULL, SEARCH_MSG)) == NULL)
		return (1);
	rp->lno = mp->lno;
	rp->cno = mp->cno + 1;		/* Offset by one. */
	return (0);
}

/*
 * v_searchb -- [count]?RE
 *	Search backward.
 */
int
v_searchb(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn('?', &ptrn))
		return (1);
	if (ptrn == NULL) {
		*rp = *fm;
		return (0);
	}
	if ((m = b_search(curf,
	    fm, ptrn, NULL, SEARCH_MSG | SEARCH_PARSE | SEARCH_SET)) == NULL)
		return (1);
	*rp = *m;
	return (0);
}

/*
 * v_searchf -- [count]/RE
 *	Search forward.
 */
int
v_searchf(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn('/', &ptrn))
		return (1);
	if (ptrn == NULL) {
		*rp = *fm;
		return (0);
	}
	if ((m = f_search(curf,
	    fm, ptrn, NULL, SEARCH_MSG | SEARCH_PARSE | SEARCH_SET)) == NULL)
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
	if (v_gb(prompt, storep, NULL, GB_BS|GB_ESC|GB_OFF))
		return (1);

	if (*storep != NULL)
		**storep = prompt;
	return (0);
}
