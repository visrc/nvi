/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 5.31 1993/03/28 19:05:45 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:05:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

static int getptrn __P((SCR *, int, u_char **));

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
	MARK *m;

	switch(sp->searchdir) {
	case BACKWARD:
		if ((m = b_search(sp, ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case FORWARD:
		if ((m = f_search(sp, ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERROR, "No previous search pattern.");
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
v_searchN(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;

	switch(sp->searchdir) {
	case BACKWARD:
		if ((m = f_search(sp, ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case FORWARD:
		if ((m = b_search(sp, ep, fm, NULL, NULL,
		    SEARCH_MSG | SEARCH_PARSE | SEARCH_TERM)) == NULL)
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERROR, "No previous search pattern.");
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
v_searchw(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	GS *gp;
	MARK *mp;
	size_t blen;
	char *bp;

#define	WORDFORMAT	"[^[:alnum:]]%s[^[:alnum:]]"

	gp = sp->gp;
	blen = vp->kbuflen + sizeof(WORDFORMAT);
	if (F_ISSET(gp, G_TMP_INUSE)) {
		if ((bp = malloc(blen)) == NULL) {
			msgq(sp, M_ERROR, "Error: %s.", strerror(errno));
			return (1);
		}
	} else {
		BINC(sp, gp->tmp_bp, gp->tmp_blen, blen);
		bp = gp->tmp_bp;
		F_SET(gp, G_TMP_INUSE);
	}
	(void)snprintf(bp, blen, WORDFORMAT, vp->keyword);
		
	mp = f_search(sp, ep, fm, (u_char *)bp, NULL,
	    SEARCH_MSG | SEARCH_TERM);

	if (bp == gp->tmp_bp)
		F_CLR(gp, G_TMP_INUSE);
	else
		free(bp);

	if (mp == NULL)
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
v_searchb(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn(sp, '?', &ptrn))
		return (1);
	if (ptrn == NULL) {
		*rp = *fm;
		return (0);
	}
	if ((m = b_search(sp, ep, fm, ptrn, NULL,
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
v_searchf(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK *m;
	u_char *ptrn;

	if (getptrn(sp, '/', &ptrn))
		return (1);
	if (ptrn == NULL) {
		*rp = *fm;
		return (0);
	}
	if ((m = f_search(sp, ep, fm, ptrn, NULL,
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
getptrn(sp, prompt, storep)
	SCR *sp;
	int prompt;
	u_char **storep;
{
	if (sp->gb(sp, prompt, storep, NULL, GB_BS|GB_ESC|GB_OFF))
		return (1);

	if (*storep != NULL)
		**storep = prompt;
	return (0);
}
