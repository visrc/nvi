/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ch.c,v 5.19 1993/01/24 18:31:16 bostic Exp $ (Berkeley) $Date: 1993/01/24 18:31:16 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "vcmd.h"

enum csearchdir { CNOTSET, FSEARCH, fSEARCH, TSEARCH, tSEARCH };
static enum csearchdir lastdir;
static int lastkey;

#define	NOPREV {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("No previous F, f, T or t search.");		\
	return (1);							\
}

#define	NOTFOUND(ch) {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("%s not found.", asciiname[ch]);			\
	return (1);							\
}

/*
 * v_chrepeat -- [count];
 *	Repeat the last F, f, T or t search.
 */
int
v_chrepeat(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	vp->character = lastkey;

	switch(lastdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		return (v_chF(vp, fm, tm, rp));
	case fSEARCH:
		return (v_chf(vp, fm, tm, rp));
	case TSEARCH:
		return (v_chT(vp, fm, tm, rp));
	case tSEARCH:
		return (v_cht(vp, fm, tm, rp));
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
v_chrrepeat(vp, fm, tm, rp)
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
		rval = v_chf(vp, fm, tm, rp);
		break;
	case fSEARCH:
		rval = v_chF(vp, fm, tm, rp);
		break;
	case TSEARCH:
		rval = v_cht(vp, fm, tm, rp);
		break;
	case tSEARCH:
		rval = v_chT(vp, fm, tm, rp);
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
v_cht(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	if (!(rval = v_chf(vp, fm, tm, rp)))
		--rp->cno;
	lastdir = tSEARCH;
	return (rval);
}
	
/*
 * v_chf -- [count]fc
 *	Search forward in the line for the next occurrence of the character.
 */
int
v_chf(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register u_char *ep, *p;
	size_t len;
	u_long cnt;
	u_char *sp;

	lastdir = fSEARCH;
	lastkey = key = vp->character;

	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) == 0)
			NOTFOUND(key);
		GETLINE_ERR(fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	sp = p;
	ep = p + len;
	p += fm->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (++p < ep && *p != key);
		if (p == ep)
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
v_chT(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	if (!(rval = v_chF(vp, fm, tm, rp)))
		++rp->cno;
	lastdir = TSEARCH;
	return (0);
}

/*
 * v_chF -- [count]Fc
 *	Search backward in the line for the next occurrence of the character.
 */
int
v_chF(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	register int key;
	register u_char *p, *ep;
	size_t len;
	u_long cnt;

	lastdir = FSEARCH;
	lastkey = key = vp->character;

	if ((p = file_gline(curf, fm->lno, &len)) == NULL) {
		if (file_lline(curf) == 0)
			NOTFOUND(key);
		GETLINE_ERR(fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	ep = p - 1;
	p += fm->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (--p > ep && *p != key);
		if (p == ep)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = (p - ep) - 1;
	return (0);
}
