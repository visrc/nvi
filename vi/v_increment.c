/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_increment.c,v 9.10 1995/02/22 09:35:59 bostic Exp $ (Berkeley) $Date: 1995/02/22 09:35:59 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

static char * const fmt[] = {
#define	DEC	0
	"%ld",
#define	SDEC	1
	"%+ld",
#define	HEXC	2
	"%#0.*lX",
#define	HEXL	3
	"%#0.*lx",
#define	OCTAL	4
	"%#0.*lo",
};

static void inc_err __P((SCR *, enum nresult));

/*
 * v_increment -- [count]#[#+-]
 *	Increment/decrement a keyword number.
 */
int
v_increment(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	enum nresult nret;
	recno_t lno;
	u_long ulval;
	long change, ltmp, lval;
	size_t beg, blen, end, len, nlen, wlen;
	int base, rval;
	char *bp, *ntype, *p, *t, nbuf[100];

	/* Validate the operator. */
	if (vp->character == '#')
		vp->character = '+';
	if (vp->character != '+' && vp->character != '-') {
		v_message(sp, vp->kp->usage, VIM_USAGE);
		return (1);
	}

	/* If new value set, save it off, but it has to fit in a long. */
	if (F_ISSET(vp, VC_C1SET)) {
		if (vp->count > LONG_MAX) {
			inc_err(sp, NUM_OVER);
			return (1);
		}
		change = vp->count;
	} else
		change = 1;

	/* Get the line. */
	if ((p = file_gline(sp, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, &lno))
			return (1);
		FILE_LERR(sp, vp->m_start.lno);
		return (1);
	}

	/*
	 * Skip any leading space before the number.  Getting a cursor word
	 * implies moving the cursor to its beginning, if we moved, refresh
	 * now.
	 */
	for (beg = vp->m_start.cno; beg < len && isspace(p[beg]); ++beg);
	if (beg >= len)
		goto nonum;
	if (beg != vp->m_start.cno) {
		sp->cno = beg;
		(void)sp->e_refresh(sp);
	}

#undef	ishex
#define	ishex(c)	(isdigit(c) || strchr("abcdefABCDEF", c))
#undef	isoctal
#define	isoctal(c)	(isdigit(c) && (c) != '8' && (c) != '9')

	/*
	 * Look for 0[Xx], or leading + or - signs, guess at the base.
	 * The character after that must be a number.  Wlen is set to
	 * the remaining characters in the line that could be part of
	 * the number.
	 */
	wlen = len - beg;
	if (p[beg] == '0' && wlen > 2 &&
	    (p[beg + 1] == 'X' || p[beg + 1] == 'x')) {
		base = 16;
		end = beg + 2;
		if (!ishex(p[end]))
			goto decimal;
		ntype = p[beg + 1] == 'X' ? fmt[HEXC] : fmt[HEXL];
	} else if (p[beg] == '0' && wlen > 1) {
		base = 8;
		end = beg + 1;
		if (!isoctal(p[end]))
			goto decimal;
		ntype = fmt[OCTAL];
	} else if (wlen >= 1 && (p[beg] == '+' || p[beg] == '-')) {
		base = 10;
		end = beg + 1;
		ntype = fmt[SDEC];
		if (!isdigit(p[end]))
			goto nonum;
	} else {
decimal:	base = 10;
		end = beg;
		ntype = fmt[DEC];
		if (!isdigit(p[end])) {
nonum:			msgq(sp, M_ERR, "213|Cursor not in a number");
			return (1);
		}
	}

	/* Find the end of the word, possibly correcting the base. */
	while (++end < len) {
		switch (base) {
		case 8:
			if (isoctal(p[end]))
				continue;
			if (p[end] == '8' || p[end] == '9') {
				base = 10;
				ntype = fmt[DEC];
				continue;
			}
			break;
		case 10:
			if (isdigit(p[end]))
				continue;
			break;
		case 16:
			if (ishex(p[end]))
				continue;
			break;
		default:
			abort();
			/* NOTREACHED */
		}
		break;
	}
	wlen = (end - beg);

	/*
	 * XXX
	 * If the line was at the end of the buffer, we have to copy it
	 * so we can guarantee that it's NULL-terminated.  We make the
	 * buffer big enough to fit the line changes as well, and only
	 * allocate once.
	 */
	GET_SPACE_RET(sp, bp, blen, len + 50);
	if (end == len) {
		memmove(bp, &p[beg], wlen);
		bp[wlen] = '\0';
		t = bp;
	} else
		t = &p[beg];

	/*
	 * Octal or hex deal in unsigned longs, everything else is done
	 * in signed longs.
	 */
	if (base == 10) {
		if ((nret = nget_slong(sp, &lval, t, NULL, 10)) != NUM_OK)
			goto err;
		ltmp = vp->character == '-' ? -change : change;
		if (lval > 0 && ltmp > 0 && !NPFITS(LONG_MAX, lval, ltmp)) {
			nret = NUM_OVER;
			goto err;
		}
		if (lval < 0 && ltmp < 0 && !NNFITS(LONG_MIN, lval, ltmp)) {
			nret = NUM_UNDER;
			goto err;
		}
		lval += ltmp;
		/* If we cross 0, signed numbers lose their sign. */
		if (lval == 0 && ntype == fmt[SDEC])
			ntype = fmt[DEC];
		nlen = snprintf(nbuf, sizeof(nbuf), ntype, lval);
	} else {
		if ((nret = nget_uslong(sp, &ulval, t, NULL, base)) != NUM_OK)
			goto err;
		if (vp->character == '+') {
			if (!NPFITS(ULONG_MAX, ulval, change)) {
				nret = NUM_OVER;
				goto err;
			}
			ulval += change;
		} else {
			if (ulval < change) {
				nret = NUM_UNDER;
				goto err;
			}
			ulval -= change;
		}
		nlen = snprintf(nbuf, sizeof(nbuf), ntype, wlen, ulval);
		/*
		 * !!!
		 * UNIX sprintf(3) functions lose the leading 0[Xx] if
		 * the number is a 0.  Not cool.
		 */
		if (base == 16 && ulval == 0) {
			nbuf[0] = '0';
			nbuf[1] = ntype == fmt[HEXC] ? 'X' : 'x';
		}
	}

	/* Build the new line. */
	memmove(bp, p, beg);
	memmove(bp + beg, nbuf, nlen);
	memmove(bp + beg + nlen, p + end, len - beg - (end - beg));
	len = beg + nlen + (len - beg - (end - beg));

	nret = NUM_OK;
	rval = file_sline(sp, vp->m_start.lno, bp, len);

	if (0) {
err:		rval = 1;
		inc_err(sp, nret);
	}
	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);
	return (rval);
}

static void
inc_err(sp, nret)
	SCR *sp;
	enum nresult nret;
{
	switch (nret) {
	case NUM_ERR:
		break;
	case NUM_OK:
		abort();
		/* NOREACHED */
	case NUM_OVER:
		msgq(sp, M_ERR, "178|Resulting number too large");
		break;
	case NUM_UNDER:
		msgq(sp, M_ERR, "179|Resulting number too small");
		break;
	}
}
