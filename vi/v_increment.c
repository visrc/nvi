/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_increment.c,v 5.11 1992/10/29 14:44:18 bostic Exp $ (Berkeley) $Date: 1992/10/29 14:44:18 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "extern.h"

static int lastch = '+';
static int lastcnt = 1;
static char *fmt[] = {
#define	DEC	0
	"%.*ld",
#define	SDEC	1
	"%+.*ld",
#define	HEXC	2
	"%#0.*lX",
#define	HEXL	3
	"%#0.*lx",
#define	OCTAL	4
	"%#0.*lo",
};

/*
 * v_increment -- [count]#[#+-]
 *	Increment/decrement a keyword number.
 */
int
v_increment(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
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

	/* Figure out the resulting type and number. */
	p = vp->keyword;
	len = vp->klen;
	if (len > 1 && p[0] == '0') {
		if (vp->character == '+') {
			ulval = strtoul(vp->keyword, NULL, 0);
			if (ULONG_MAX - ulval < lastcnt)
				goto overflow;
			ulval += lastcnt;
		} else {
			ulval = strtoul(vp->keyword, NULL, 0);
			if (ulval < lastcnt)
				goto underflow;
			ulval -= lastcnt;
		}
		ntype = fmt[OCTAL];
		if (len > 2)
			if (p[1] == 'X')
				ntype = fmt[HEXC];
			else if (p[1] == 'x')
				ntype = fmt[HEXL];
		(void)snprintf(nbuf, sizeof(nbuf), ntype, len, ulval);
	} else {
		if (vp->character == '+') {
			lval = strtol(vp->keyword, NULL, 0);
			if (LONG_MAX - lval < lastcnt) {
overflow:			bell();
				msg("Resulting number too large.");
				return (1);
			}
			lval += lastcnt;
		} else {
			lval = strtol(vp->keyword, NULL, 0);
			if (lval < 0 && -(LONG_MIN - lval) > lastcnt) {
underflow:			bell();
				msg("Resulting number too small.");
				return (1);
			}
			lval -= lastcnt;
		}
		ntype = *vp->keyword == '+' || *vp->keyword == '-' ?
			fmt[SDEC] : fmt[DEC];
		(void)snprintf(nbuf, sizeof(nbuf), ntype, len, lval);
	}
	len = strlen(nbuf);

	*rp = *fm;
	ecursor = *fm;
	ecursor.cno += vp->klen;
			
	return (change(fm, &ecursor, (u_char *)nbuf, len));
}
