/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 8.3 1993/08/25 16:49:20 bostic Exp $ (Berkeley) $Date: 1993/08/25 16:49:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_replace -- [count]rc
 *	The r command in historic vi was almost beautiful in its badness.
 *	For example, "r<erase>" and "r<word erase>" beeped the terminal
 *	and deleted a single character.  "Nr<carriage return>", where N
 *	was greater than 1, inserted a single carriage return.  This may
 *	not be right, but at least it's not insane.
 */
int
v_replace(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	TEXT *tp;
	size_t blen, len;
	recno_t lno;
	u_long cnt;
	int rval, ch;
	char *bp, *p;

	/*
	 * If the line doesn't exist, or it's empty, replacement isn't
	 * allowed.  It's not hard to implement, but:
	 *
	 *	1: It's historic practice.
	 *	2: For consistency, this change would require that the more
	 *	   general case, "Nr", when the user is < N characters from
	 *	   the end of the line, also work.
	 *	3: Replacing a newline has somewhat odd semantics.
	 */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		goto nochar;
	}
	if (len == 0) {
nochar:		msgq(sp, M_BERR, "No characters to replace");
		return (1);
	}

	/*
	 * Figure out how many characters to be replace; for no particular
	 * reason other than that the semantics of replacing the newline
	 * are confusing, only permit the replacement of the characters in
	 * the current line.  I suppose we could simply append the replacement
	 * characters to the line, but I see no compelling reason to do so.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	rp->cno = fm->cno + cnt - 1;
	if (rp->cno > len - 1) {
		v_eol(sp, ep, fm);
		return (1);
	}

	/* Get the character. */
	if (F_ISSET(vp, VC_ISDOT))
		ch = sp->rlast;
	else
		ch = sp->rlast = term_key(sp, 0);

	/* Copy the line. */
	GET_SPACE(sp, bp, blen, len);
	memmove(bp, p, len);
	p = bp;

	if (sp->special[ch] == K_CR || sp->special[ch] == K_NL) {
		/* Set return line. */
		rp->lno = fm->lno + cnt;

		/* The first part of the current line. */
		if (file_sline(sp, ep, fm->lno, p, fm->cno))
			return (1);

		/*
		 * The rest of the current line.  And, of course, now it gets
		 * tricky.  Any white space after the replaced character is
		 * stripped, and autoindent is applied.  Put the cursor on the
		 * last indent character as did historic vi.
		 */
		for (p += fm->cno + cnt, len -= fm->cno + cnt;
		    len && isblank(*p); --len, ++p);

		if ((tp = text_init(sp, p, len, len)) == NULL)
			return (1);
		if (txt_auto(sp, ep, fm->lno, NULL, tp))
			return (1);
		rp->cno = tp->ai ? tp->ai - 1 : 0;
		if (file_aline(sp, ep, 1, fm->lno, tp->lb, tp->len))
			return (1);
		text_free(tp);
		
		/* All of the middle lines. */
		while (--cnt)
			if (file_aline(sp, ep, 1, fm->lno, "", 0))
				return (1);
		return (0);
	}

	memset(bp + fm->cno, ch, cnt);
	rval = file_sline(sp, ep, fm->lno, bp, len);

	rp->lno = fm->lno;
	rp->cno = fm->cno + cnt - 1;

	FREE_SPACE(sp, bp, blen);
	return (rval);
}
