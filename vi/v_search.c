/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 8.5 1993/09/02 11:33:38 bostic Exp $ (Berkeley) $Date: 1993/09/02 11:33:38 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

static int getptrn __P((SCR *, EXF *, int, char **));
static int shift_b __P((SCR *, EXF *, MARK *));
static int shift_f __P((SCR *, EXF *, MARK *));

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
		if (F_ISSET(vp, VC_SH) && shift_b(sp, ep, rp))
			return (1);
		break;
	case FORWARD:
		if (f_search(sp, ep, fm, rp, NULL, NULL, flags))
			return (1);
		if (F_ISSET(vp, VC_SH) && shift_f(sp, ep, rp))
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
		if (F_ISSET(vp, VC_SH) && shift_f(sp, ep, rp))
			return (1);
		break;
	case FORWARD:
		if (b_search(sp, ep, fm, rp, NULL, NULL, flags))
			return (1);
		if (F_ISSET(vp, VC_SH) && shift_b(sp, ep, rp))
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
	if (rval)
		return (1);
	if (F_ISSET(vp, VC_SH) && shift_f(sp, ep, rp))
		return (1);
	return (0);
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
	if (F_ISSET(vp, VC_SH) && shift_b(sp, ep, rp))
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
	if (F_ISSET(vp, VC_SH) && shift_f(sp, ep, rp))
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

/*
 * shift_b --
 *	Handle shift command with a backward search as the motion.
 *
 * Historically, shift commands don't shift the line searched to if the
 * pattern matched was the start or end of the line.  It seems like this
 * could be handled elsewhere in a general fashion, but I've so far been
 * unable to figure out any pattern.
 */
static int
shift_b(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	size_t len;
	char *p;

	if ((p = file_gline(sp, ep, rp->lno + 1, &len)) == NULL) {
		GETLINE_ERR(sp, rp->lno);
		return (1);
	}
	if (len == 0 || rp->cno >= len) {
		++rp->lno;
		rp->cno = 0;
	}
	return (0);
}

/*
 * shift_f --
 *	Handle shift command with a forward search as the motion.
 */
static int
shift_f(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	size_t len;
	char *p;

	if (rp->cno != 0)
		return (0);
	if ((p = file_gline(sp, ep, --rp->lno, &len)) == NULL) {
		GETLINE_ERR(sp, rp->lno);
		return (1);
	}
	rp->cno = len;
	return (0);
}
