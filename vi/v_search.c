/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 5.28 1993/02/28 14:01:56 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:01:56 $";
#endif /* not lint */

#include <sys/param.h>

#include <curses.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "term.h"
#include "vcmd.h"

static int getptrn __P((EXF *, int, u_char **));

/*
 * v_searchn -- n
 *	Repeat last search.
 */
int
v_searchn(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;

	switch(ep->searchdir) {
	case BACKWARD:
		if ((m = b_search(ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case FORWARD:
		if ((m = f_search(ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case NOTSET:
		ep->msg(ep, M_ERROR, "No previous search pattern.");
		return (1);
	default:
		abort();
	}
	*rp = *m;
	return (0);
}

/*
 * v_searchN -- N
 *	Reverse last search.
 */
int
v_searchN(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;

	switch(ep->searchdir) {
	case BACKWARD:
		if ((m = f_search(ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case FORWARD:
		if ((m = b_search(ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case NOTSET:
		ep->msg(ep, M_ERROR, "No previous search pattern.");
		return (1);
	default:
		abort();
	}
	*rp = *m;
	return (0);
}

/*
 * v_searchw -- [count]^A
 *	Search for the word under the cursor.
 */
int
v_searchw(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *mp;
	static u_char *wbuf;
	static size_t wbuflen;

#define	WORDFORMAT	"[^[:alnum:]]%s[^[:alnum:]]"

	BINC(ep, wbuf, wbuflen, vp->kbuflen + sizeof(WORDFORMAT));
	(void)snprintf((char *)wbuf, wbuflen, WORDFORMAT, vp->keyword);
		
	if ((mp = f_search(ep, fm,
	    (u_char *)wbuf, NULL, SEARCH_MSG | SEARCH_TERM)) == NULL)
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
v_searchb(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn(ep, '?', &ptrn))
		return (1);
	if (ptrn == NULL) {
		*rp = *fm;
		return (0);
	}
	if ((m = b_search(ep, fm, ptrn, NULL,
	    SEARCH_MSG | SEARCH_PARSE | SEARCH_SET | SEARCH_TERM)) == NULL)
		return (1);
	*rp = *m;
	return (0);
}

/*
 * v_searchf -- [count]/RE
 *	Search forward.
 */
int
v_searchf(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn(ep, '/', &ptrn))
		return (1);
	if (ptrn == NULL) {
		*rp = *fm;
		return (0);
	}
	if ((m = f_search(ep, fm, ptrn, NULL,
	    SEARCH_MSG | SEARCH_PARSE | SEARCH_SET | SEARCH_TERM)) == NULL)
		return (1);
	*rp = *m;
	return (0);
}

/*
 * getptrn --
 *	Get the search pattern.
 */
static int
getptrn(ep, prompt, storep)
	EXF *ep;
	int prompt;
	u_char **storep;
{
	if (v_gb(ep, prompt, storep, NULL, GB_BS|GB_ESC|GB_OFF))
		return (1);

	if (*storep != NULL)
		**storep = prompt;
	return (0);
}
