/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ch.c,v 5.21 1993/02/28 14:01:45 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:01:45 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"

enum csearchdir { CNOTSET, FSEARCH, fSEARCH, TSEARCH, tSEARCH };
static enum csearchdir lastdir;
static int lastkey;

#define	NOPREV {							\
	ep->msg(ep, M_BELL, "No previous F, f, T or t search.");	\
	return (1);							\
}

#define	NOTFOUND(ch) {							\
	ep->msg(ep, M_BELL, "%s not found.", asciiname[ch]);		\
	return (1);							\
}

/*
 * v_chrepeat -- [count];
 *	Repeat the last F, f, T or t search.
 */
int
v_chrepeat(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	vp->character = lastkey;

	switch(lastdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		return (v_chF(ep, vp, fm, tm, rp));
	case fSEARCH:
		return (v_chf(ep, vp, fm, tm, rp));
	case TSEARCH:
		return (v_chT(ep, vp, fm, tm, rp));
	case tSEARCH:
		return (v_cht(ep, vp, fm, tm, rp));
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
v_chrrepeat(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;
	enum csearchdir savedir;

	vp->character = lastkey;
	savedir = lastdir;

	switch(lastdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		rval = v_chf(ep, vp, fm, tm, rp);
		break;
	case fSEARCH:
		rval = v_chF(ep, vp, fm, tm, rp);
		break;
	case TSEARCH:
		rval = v_cht(ep, vp, fm, tm, rp);
		break;
	case tSEARCH:
		rval = v_chT(ep, vp, fm, tm, rp);
		break;
	default:
		abort();
	}
	lastdir = savedir;
	return (rval);
}

/*
 * v_cht -- [count]tc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor to its left.
 */
int
v_cht(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	if (!(rval = v_chf(ep, vp, fm, tm, rp)))
		--rp->cno;
	lastdir = tSEARCH;
	return (rval);
}
	
/*
 * v_chf -- [count]fc
 *	Search forward in the line for the next occurrence of the character.
 */
int
v_chf(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register u_char *endp, *p;
	size_t len;
	u_long cnt;
	u_char *sp;

	lastdir = fSEARCH;
	lastkey = key = vp->character;

	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) == 0)
			NOTFOUND(key);
		GETLINE_ERR(ep, fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	sp = p;
	endp = p + len;
	p += fm->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (++p < endp && *p != key);
		if (p == endp)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = p - sp;
	return (0);
}

/*
 * v_chT -- [count]Tc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor to its right.
 */
int
v_chT(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	if (!(rval = v_chF(ep, vp, fm, tm, rp)))
		++rp->cno;
	lastdir = TSEARCH;
	return (0);
}

/*
 * v_chF -- [count]Fc
 *	Search backward in the line for the next occurrence of the character.
 */
int
v_chF(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register u_char *p, *endp;
	size_t len;
	u_long cnt;

	lastdir = FSEARCH;
	lastkey = key = vp->character;

	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) == 0)
			NOTFOUND(key);
		GETLINE_ERR(ep, fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	endp = p - 1;
	p += fm->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (--p > endp && *p != key);
		if (p == endp)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = (p - endp) - 1;
	return (0);
}
