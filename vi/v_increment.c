/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_increment.c,v 5.5 1992/05/22 09:58:45 bostic Exp $ (Berkeley) $Date: 1992/05/22 09:58:45 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

static int lastch = '+';
static int lastcnt = 1;
static char *fmt[] = {
#define	DECIMAL	0
	"%.*ld",
#define	HEXC	1
	"%#0.*lX",
#define	HEXL	2
	"%#0.*lx",
#define	OCTAL	3
	"%#0.*lo",
};

/*
 * v_increment -- [count]#[#+-]
 *	Increment/decrement a keyword number.
 */
int
v_increment(vp, cp, rp)
	VICMDARG *vp;
	MARK *cp, *rp;
{
	MARK ecursor;
	u_long ulval;
	long lval;
	size_t len;
	char *p, *ntype, nbuf[40];

	/* Do repeat operations. */
	if (vp->character == '#')
		vp->character = lastch;

	/* Get new value. */
	if (vp->flags & VC_C1SET)
		lastcnt = vp->count;

	if (vp->character != '+' && vp->character != '-') {
		bell();
		msg("usage: %s.", vp->kp->usage);
		return (1);
	}
	lastch = vp->character;

	/* Figure out the resulting type. */
	p = vp->keyword;
	len = vp->klen;
	if (len > 1 && p[0] == '0') {
		if (vp->character == '+')
			ulval = strtoul(vp->keyword, NULL, 0) + lastcnt;
		else
			ulval = strtoul(vp->keyword, NULL, 0) - lastcnt;
		ntype = fmt[OCTAL];
		if (len > 2)
			if (p[1] == 'X')
				ntype = fmt[HEXC];
			else if (p[1] == 'x')
				ntype = fmt[HEXL];
		len = snprintf(nbuf, sizeof(nbuf), ntype, len, ulval);
	} else {
		if (vp->character == '+')
			lval = strtol(vp->keyword, NULL, 0) + lastcnt;
		else
			lval = strtol(vp->keyword, NULL, 0) - lastcnt;
		len = snprintf(nbuf, sizeof(nbuf), fmt[DECIMAL], len, lval);
	}

	*rp = *cp;
	ecursor = *cp;
	ecursor.cno += vp->klen;
			
	return (change(cp, &ecursor, nbuf, len));
}
