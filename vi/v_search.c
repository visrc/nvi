/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 8.4 1993/08/23 12:31:49 bostic Exp $ (Berkeley) $Date: 1993/08/23 12:31:49 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

static int getptrn __P((SCR *, EXF *, int, char **));

/*
 * v_searchn -- n
 *	Repeat last search.
 */
int
v_searchn(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int flags;

	flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM;
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		flags |= SEARCH_EOL;
	switch (sp->searchdir) {
	case BACKWARD:
		if (b_search(sp, ep, fm, rp, NULL, NULL, flags))
			return (1);
		break;
	case FORWARD:
		if (f_search(sp, ep, fm, rp, NULL, NULL, flags))
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERR, "No previous search pattern.");
		return (1);
	default:
		abort();
	}
	return (0);
}

/*
 * v_searchN -- N
 *	Reverse last search.
 */
int
v_searchN(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int flags;

	flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM;
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		flags |= SEARCH_EOL;
	switch (sp->searchdir) {
	case BACKWARD:
		if (f_search(sp, ep, fm, rp, NULL, NULL, flags))
			return (1);
		break;
	case FORWARD:
		if (b_search(sp, ep, fm, rp, NULL, NULL, flags))
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERR, "No previous search pattern.");
		return (1);
	default:
		abort();
	}
	return (0);
}

/*
 * v_searchw -- [count]^A
 *	Search for the word under the cursor.
 */
int
v_searchw(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t blen, len;
	int rval;
	char *bp;

	len = vp->kbuflen + sizeof(RE_WSTART) + sizeof(RE_WSTOP);
	GET_SPACE(sp, bp, blen, len);
	(void)snprintf(bp, blen, "%s%s%s", RE_WSTART, vp->keyword, RE_WSTOP);
		
	rval = f_search(sp, ep, fm, rp, bp, NULL, SEARCH_MSG | SEARCH_TERM);

	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * v_searchb -- [count]?RE?offset
 *	Search backward.
 */
int
v_searchb(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int flags;
	char *ptrn;

	if (getptrn(sp, ep, '?', &ptrn))
		return (1);

	flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_SET;
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		flags |= SEARCH_EOL;
	if (b_search(sp, ep, fm, rp, ptrn, NULL, flags))
		return (1);
	return (0);
}

/*
 * v_searchf -- [count]/RE/offset
 *	Search forward.
 */
int
v_searchf(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int flags;
	char *ptrn;

	if (getptrn(sp, ep, '/', &ptrn))
		return (1);

	flags = SEARCH_MSG | SEARCH_PARSE | SEARCH_SET;
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		flags |= SEARCH_EOL;
	if (f_search(sp, ep, fm, rp, ptrn, NULL, flags))
		return (1);
	return (0);
}

/*
 * getptrn --
 *	Get the search pattern.
 */
static int
getptrn(sp, ep, prompt, storep)
	SCR *sp;
	EXF *ep;
	int prompt;
	char **storep;
{
	TEXT *tp;

	if (sp->s_get(sp, ep, &sp->bhdr, prompt,
	    TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT))
		return (1);

	tp = sp->bhdr.next;
	if (tp->len == 1)
		*storep = NULL;
	else
		*storep = tp->lb;
	return (0);
}
