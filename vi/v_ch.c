/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ch.c,v 8.1 1993/06/09 22:26:44 bostic Exp $ (Berkeley) $Date: 1993/06/09 22:26:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>

#include "vi.h"
#include "vcmd.h"

#define	NOPREV {							\
	msgq(sp, M_BERR, "No previous F, f, T or t search.");		\
	return (1);							\
}

#define	NOTFOUND(ch) {							\
	msgq(sp, M_BERR, "%s not found.", charname(sp, ch));		\
	return (1);							\
}

/*
 * v_chrepeat -- [count];
 *	Repeat the last F, f, T or t search.
 */
int
v_chrepeat(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	vp->character = sp->lastckey;

	switch (sp->csearchdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		return (v_chF(sp, ep, vp, fm, tm, rp));
	case fSEARCH:
		return (v_chf(sp, ep, vp, fm, tm, rp));
	case TSEARCH:
		return (v_chT(sp, ep, vp, fm, tm, rp));
	case tSEARCH:
		return (v_cht(sp, ep, vp, fm, tm, rp));
	default:
		abort();
	}
	/* NOTREACHED */
}

/*
 * v_chrrepeat -- [count],
 *	Repeat the last F, f, T or t search in the reverse direction.
 */
int
v_chrrepeat(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;
	enum cdirection savedir;

	vp->character = sp->lastckey;
	savedir = sp->csearchdir;

	switch (sp->csearchdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		rval = v_chf(sp, ep, vp, fm, tm, rp);
		break;
	case fSEARCH:
		rval = v_chF(sp, ep, vp, fm, tm, rp);
		break;
	case TSEARCH:
		rval = v_cht(sp, ep, vp, fm, tm, rp);
		break;
	case tSEARCH:
		rval = v_chT(sp, ep, vp, fm, tm, rp);
		break;
	default:
		abort();
	}
	sp->csearchdir = savedir;
	return (rval);
}

/*
 * v_cht -- [count]tc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor on it if a motion command, or to its left if not.
 */
int
v_cht(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	rval = v_chf(sp, ep, vp, fm, tm, rp);
	if (!rval)
		--rp->cno;	/* XXX: Motion interaction with v_chf. */
	sp->csearchdir = tSEARCH;
	return (rval);
}
	
/*
 * v_chf -- [count]fc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor to it's right if a motion command, or on it if not.
 */
int
v_chf(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register char *endp, *p;
	size_t len;
	recno_t lno;
	u_long cnt;
	char *startp;

	sp->csearchdir = fSEARCH;
	sp->lastckey = key = vp->character;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			NOTFOUND(key);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	startp = p;
	endp = p + len;
	p += fm->cno;
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		while (++p < endp && *p != key);
		if (p == endp)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = p - startp;
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		++rp->cno;
	return (0);
}

/*
 * v_chT -- [count]Tc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor to its right.
 */
int
v_chT(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	rval = v_chF(sp, ep, vp, fm, tm, rp);
	if (!rval)
		++rp->cno;
	sp->csearchdir = TSEARCH;
	return (0);
}

/*
 * v_chF -- [count]Fc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor on it.
 */
int
v_chF(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register char *p, *endp;
	recno_t lno;
	size_t len;
	u_long cnt;

	sp->csearchdir = FSEARCH;
	sp->lastckey = key = vp->character;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			NOTFOUND(key);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	endp = p - 1;
	p += fm->cno;
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		while (--p > endp && *p != key);
		if (p == endp)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = (p - endp) - 1;
	return (0);
}
