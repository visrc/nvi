/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ch.c,v 5.10 1992/05/20 10:45:46 bostic Exp $ (Berkeley) $Date: 1992/05/20 10:45:46 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"
#include "extern.h"

enum csearchdir { NOTSET, FSEARCH, fSEARCH, TSEARCH, tSEARCH };
static enum csearchdir lastdir;
static int lastkey;

#define	NOPREV {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("No previous F, f, T or t search.");		\
	return (1);							\
}

#define	NOTFOUND {							\
	bell();								\
	if (ISSET(O_VERBOSE))						\
		msg("Character not found.");				\
	return (1);							\
}

/*
 * v_repeatch -- [count];
 *	Repeat the last F, f, T or t search.
 */
int
v_repeatch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	vp->character = lastkey;

	switch(lastdir) {
	case NOTSET:
		NOPREV;
	case FSEARCH:
		return (v_Fch(vp, cp, rp));
	case fSEARCH:
		return (v_fch(vp, cp, rp));
	case TSEARCH:
		return (v_Tch(vp, cp, rp));
	case tSEARCH:
		return (v_tch(vp, cp, rp));
	}
	/* NOTREACHED */
}

/*
 * v_rrepeatch -- [count],
 *	Repeat the last F, f, T or t search in the reverse direction.
 */
int
v_rrepeatch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	int rval;
	enum csearchdir savedir;

	vp->character = lastkey;
	savedir = lastdir;

	switch(lastdir) {
	case NOTSET:
		NOPREV;
	case FSEARCH:
		rval = v_fch(vp, cp, rp);
		break;
	case fSEARCH:
		rval = v_Fch(vp, cp, rp);
		break;
	case TSEARCH:
		rval = v_tch(vp, cp, rp);
		break;
	case tSEARCH:
		rval = v_Tch(vp, cp, rp);
		break;
	}
	lastdir = savedir;
	return (rval);
}

/*
 * v_tch -- [count]tc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor to its left.
 */
int
v_tch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	int rval;

	if (!(rval = v_fch(vp, cp, rp)))
		--rp->cno;
	lastdir = tSEARCH;
	return (rval);
}
	
/*
 * v_fch -- [count]fc
 *	Search forward in the line for the next occurrence of the character.
 */
int
v_fch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	register int key;
	register char *ep, *p;
	size_t len;
	u_long cnt;
	char *sp;

	lastdir = fSEARCH;
	lastkey = key = vp->character;

	EGETLINE(p, cp->lno, len);
	if (len == 0)
		NOTFOUND;

	sp = p;
	ep = p + len;
	p += cp->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (++p < ep && *p != key);
		if (p == ep)
			NOTFOUND;
	}
	rp->lno = cp->lno;
	rp->cno = p - sp;
	return (0);
}

/*
 * v_Tch -- [count]Tc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor to its right.
 */
int
v_Tch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	int rval;

	if (!(rval = v_Fch(vp, cp, rp)))
		++rp->cno;
	lastdir = TSEARCH;
	return (0);
}

/*
 * v_Fch -- [count]Fc
 *	Search backward in the line for the next occurrence of the character.
 */
int
v_Fch(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	register int key;
	register char *p, *ep;
	size_t len;
	u_long cnt;

	lastdir = FSEARCH;
	lastkey = key = vp->character;

	EGETLINE(p, cp->lno, len);
	if (len == 0)
		NOTFOUND;

	ep = p - 1;
	p += cp->cno;
	for (cnt = vp->flags & VC_C1SET ? vp->count : 1; cnt--;) {
		while (--p > ep && *p != key);
		if (p == ep)
			NOTFOUND;
	}
	rp->lno = cp->lno;
	rp->cno = (p - ep) - 1;
	return (0);
}
