/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_increment.c,v 5.4 1992/05/15 11:14:12 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:14:12 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_increment -- [count]#[#+-]
 *	Increment/decrement a keyword number.
 */
int
v_increment(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	enum { DECIMAL, HEXC, HEXL, OCTAL} ntype;
	MARK ecursor;
	u_long val;
	static int lastch = '+';
	static int lastcnt = 1;
	size_t len;
	char *p, *ep, nbuf[40];

	/* Do repeat operations. */
	if (vp->character == '#')
		vp->character = lastch;

	/* Get new value. */
	val = vp->flags & VC_C1SET ? vp->count : lastcnt;
	switch(vp->character) {
	case '+':
		val += strtol(vp->keyword, &ep, 0);
		break;
	case '-':
		val -= strtol(vp->keyword, &ep, 0);
		break;
	default:
		bell();
		msg("usage: %s.", vp->kp->usage);
		return (1);
	}

	if (*ep) {
		bell();
		msg("Cursor not in a number.");
		return (1);
	}

	/* Figure out the resulting type. */
	p = vp->keyword;
	len = vp->klen;
	if (len > 1 && *p == '0') {
		ntype = OCTAL;
		++p;
		--len;
		if (len > 1)
			if (*p == 'X') {
				ntype = HEXC;
				++p;
				--len;
			} else if (*p == 'x') {
				ntype = HEXL;
				++p;
				--len;
			}
	} else
		ntype = DECIMAL;

	/* Build a new number. */
	switch(ntype) {
	case DECIMAL:
		len = snprintf(nbuf, sizeof(nbuf), "%0.*lu", len, val);
		break;
	case HEXC:
		len = snprintf(nbuf, sizeof(nbuf), "%#0.*lX", len, val);
		break;
	case HEXL:
		len = snprintf(nbuf, sizeof(nbuf), "%#0.*lx", len, val);
		break;
	case OCTAL:
		len = snprintf(nbuf, sizeof(nbuf), "%#0.*lo", len, val);
		break;
	}

	ecursor = *cp;
	ecursor.cno += vp->klen;
			
	return (change(cp, &ecursor, nbuf, len));
}
