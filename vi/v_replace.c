/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 5.21 1993/04/18 09:34:35 bostic Exp $ (Berkeley) $Date: 1993/04/18 09:34:35 $";
#endif /* not lint */

#include <sys/types.h>

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
 *	not be "right", but at least it's not insane.
 */
int
v_replace(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	GS *gp;
	TEXT *tp;
	size_t len;
	u_long cnt;
	int rval;
	char *bp, *p;

	/*
	 * If the line doesn't exist, or it's empty, replacement isn't
	 * allowed.  It's not hard to implement, but replacing "nothing"
	 * has odd semantics.
	 */
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep) != 0) {
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
	 * the current line.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	rp->cno = fm->cno + cnt - 1;
	if (rp->cno > len - 1) {
		v_eol(sp, ep, fm);
		return (1);
	}

	if (sp->special[vp->character] == K_CR ||
	    sp->special[vp->character] == K_NL) {
		/* Set return line. */
		rp->lno = fm->lno + cnt;

		/* The first part of the current line. */
		if (file_sline(sp, ep, fm->lno, p, fm->cno))
			return (1);

		/*
		 * The rest of the current line.  This one has autoindent
		 * applied.  Historic vi put the cursor on the last indent
		 * character.  There's almost certainly no reason to do
		 * that, but I can't think of a reason not to, either.
		 */
		p += fm->cno + cnt;
		len -= fm->cno + cnt;
		if ((tp = text_init(sp, p, len, len)) == NULL)
			return (1);
		if (txt_auto(sp, ep, fm->lno, tp))
			return (1);
		rp->cno = tp->ai ? tp->ai - 1 : 0;
		if (file_aline(sp, ep, fm->lno, tp->lb, tp->len))
			return (1);
		free(tp->lb);
		free(tp);
		
		/* All of the middle lines. */
		while (--cnt)
			if (file_aline(sp, ep, fm->lno, "", 0))
				return (1);
		return (0);
	}

	gp = sp->gp;
	if (F_ISSET(gp, G_TMP_INUSE)) {
		if ((bp = malloc(len)) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			return (1);
		}
	} else {
		BINC(sp, gp->tmp_bp, gp->tmp_blen, len);
		bp = gp->tmp_bp;
		F_SET(gp, G_TMP_INUSE);
	}

	memmove(bp, p, len);
	memset(bp + fm->cno, vp->character, cnt);
	rval = file_sline(sp, ep, fm->lno, bp, len);

	rp->lno = fm->lno;
	rp->cno = fm->cno + cnt - 1;

	if (bp == gp->tmp_bp)
		F_CLR(gp, G_TMP_INUSE);
	else
		free(bp);

	F_SET(sp, S_CUR_INVALID);		/* Chars deleted. */

	return (rval);
}
