/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_ulcase.c,v 10.1 1995/03/16 20:39:43 bostic Exp $ (Berkeley) $Date: 1995/03/16 20:39:43 $";
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

#include "common.h"
#include "vi.h"

static int ulcase __P((SCR *, recno_t, CHAR_T *, size_t, size_t, size_t));

/*
 * v_ulcase -- [count]~
 *	Toggle upper & lower case letters.
 *
 * !!!
 * Historic vi didn't permit ~ to cross newline boundaries.  I can
 * think of no reason why it shouldn't, which at least lets the user
 * auto-repeat through a paragraph.
 *
 * !!!
 * In historic vi, the count was ignored.  It would have been better
 * if there had been an associated motion, but it's too late to make
 * that the default now.
 */
int
v_ulcase(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	recno_t lno;
	size_t cno, lcnt, len;
	u_long cnt;
	char *p;

	lno = vp->m_start.lno;
	cno = vp->m_start.cno;

	/* EOF is an infinite count sink. */
	for (cnt =
	    F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt > 0; cno = 0, ++lno) {
		if ((p = file_gline(sp, lno, &len)) == NULL)
			break;

		/* Empty lines decrement the count by one. */
		if (len == 0) {
			--cnt;
			vp->m_final.lno = lno + 1;
			vp->m_final.cno = 0;
		} else {
			if (cno + cnt >= len) {
				lcnt = len - 1;
				cnt -= len - cno;

				vp->m_final.lno = lno + 1;
				vp->m_final.cno = 0;
			} else {
				lcnt = cno + cnt - 1;
				cnt = 0;

				vp->m_final.lno = lno;
				vp->m_final.cno = lcnt + 1;
			}

			if (ulcase(sp, lno, p, len, cno, lcnt))
				return (1);
		}
	}

	/* Check to see if we tried to move past EOF. */
	if (!file_eline(sp, vp->m_final.lno)) {
		(void)file_gline(sp, --vp->m_final.lno, &len);
		vp->m_final.cno = len == 0 ? 0 : len - 1;
	}
	return (0);
}

/*
 * v_mulcase -- [count]~[count]motion
 *	Toggle upper & lower case letters over a range.
 */
int
v_mulcase(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	CHAR_T *p;
	size_t len;
	recno_t lno;

	for (lno = vp->m_start.lno;;) {
		if ((p = file_gline(sp, lno, &len)) == NULL) {
			FILE_LERR(sp, lno);
			return (1);
		}
		if (len != 0 && ulcase(sp, lno, p, len,
		    lno == vp->m_start.lno ? vp->m_start.cno : 0,
		    !F_ISSET(vp, VM_LMODE) &&
		    lno == vp->m_stop.lno ? vp->m_stop.cno : len))
			return (1);

		if (++lno > vp->m_stop.lno)
			break;
	}

	/*
	 * XXX
	 * I didn't create a new motion command when I added motion semantics
	 * for ~.  While that's the correct way to do it, that choice would
	 * have required changes all over the vi directory for little gain.
	 * Instead, we pretend it's a yank command.  Note, this means that we
	 * follow the cursor motion rules for yank commands, but that seems
	 * reasonable to me.
	 */
	return (0);
}

/*
 * ulcase --
 *	Change part of a line's case.
 */
static int
ulcase(sp, lno, lp, len, scno, ecno)
	SCR *sp;
	recno_t lno;
	CHAR_T *lp;
	size_t len, scno, ecno;
{
	size_t blen;
	int change, rval;
	CHAR_T ch, *p, *t;
	char *bp;

	GET_SPACE_RET(sp, bp, blen, len);
	memmove(bp, lp, len);

	change = rval = 0;
	for (p = bp + scno, t = bp + ecno + 1; p < t; ++p) {
		ch = *(u_char *)p;
		if (islower(ch)) {
			*p = toupper(ch);
			change = 1;
		} else if (isupper(ch)) {
			*p = tolower(ch);
			change = 1;
		}
	}

	if (change && file_sline(sp, lno, bp, len))
		rval = 1;

	FREE_SPACE(sp, bp, blen);
	return (rval);
}
