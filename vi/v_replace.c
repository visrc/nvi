/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_replace.c,v 10.3 1995/06/08 19:01:59 bostic Exp $ (Berkeley) $Date: 1995/06/08 19:01:59 $";
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

static int v_rcheck __P((SCR *, recno_t, char **, size_t *));

/*
 * v_replace -- [count]r<char>
 *
 * !!!
 * The r command in historic vi was almost beautiful in its badness.  For
 * example, "r<erase>" and "r<word erase>" beeped the terminal and deleted
 * a single character.  "Nr<carriage return>", where N was greater than 1,
 * inserted a single carriage return.  "r<escape>" did cancel the command,
 * but "r<literal><escape>" erased a single character.  To enter a literal
 * <literal> character, it required three <literal> characters after the
 * command.  This may not be right, but at least it's not insane.
 *
 * PUBLIC: int v_replace __P((SCR *, VICMD *));
 */
int
v_replace(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	char *p;
	size_t len;

	/* If it's dot, just make the replacement. */
	if (F_ISSET(vp, VC_ISDOT))
		return (v_replace_td(sp, vp));
		
	/* Check for legality. */
	if (v_rcheck(sp, vp->m_start.lno, &p, &len))
		return (1);

	/* Update the mode line. */
	sp->showmode = SM_REPLACE;

	/* Get the replacement character. */
	VIP(sp)->cm_state = VS_REPLACE_CHAR1;
	return (0);
}

/*
 * v_replace_td --
 *	Do the replacement.
 *
 * PUBLIC: int v_replace_td __P((SCR *, VICMD *));
 */
int
v_replace_td(sp, vp)
	SCR *sp;
	VICMD *vp;
{
	VI_PRIVATE *vip;
	TEXT *tp;
	size_t blen, len;
	u_long cnt;
	int rval;
	char *bp, *p;

	/* There's no way the line could have moved... yeah, right. */
	if (v_rcheck(sp, vp->m_start.lno, &p, &len))
		return (1);

	/*
	 * Figure out how many characters to be replace.  For no particular
	 * reason (other than that the semantics of replacing the newline
	 * are confusing) only permit the replacement of the characters in
	 * the current line.  I suppose we could append replacement characters
	 * to the line, but I see no compelling reason to do so.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno = vp->m_start.cno + cnt - 1;
	if (vp->m_stop.cno > len - 1) {
		v_eol(sp, &vp->m_start);
		return (1);
	}

	/* Copy the line. */
	GET_SPACE_RET(sp, bp, blen, len);
	memmove(bp, p, len);
	p = bp;

	vip = VIP(sp);
	if (vip->rvalue == K_CR || vip->rvalue == K_NL) {
		/* Set return line. */
		vp->m_stop.lno = vp->m_start.lno + cnt;
		vp->m_stop.cno = 0;

		/* The first part of the current line. */
		if (file_sline(sp, vp->m_start.lno, p, vp->m_start.cno))
			goto err_ret;

		/*
		 * The rest of the current line.  And, of course, now it gets
		 * tricky.  Any white space after the replaced character is
		 * stripped, and autoindent is applied.  Put the cursor on the
		 * last indent character as did historic vi.
		 */
		for (p += vp->m_start.cno + cnt, len -= vp->m_start.cno + cnt;
		    len && isblank(*p); --len, ++p);

		if ((tp = text_init(sp, p, len, len)) == NULL)
			goto err_ret;
		if (v_txt_auto(sp, vp->m_start.lno, NULL, 0, tp))
			goto err_ret;
		vp->m_stop.cno = tp->ai ? tp->ai - 1 : 0;
		if (file_aline(sp, 1, vp->m_start.lno, tp->lb, tp->len))
			goto err_ret;
		text_free(tp);

		rval = 0;

		/* All of the middle lines. */
		while (--cnt)
			if (file_aline(sp, 1, vp->m_start.lno, "", 0)) {
err_ret:			rval = 1;
				break;
			}
	} else {
		memset(bp + vp->m_start.cno, vip->rlast, cnt);
		rval = file_sline(sp, vp->m_start.lno, bp, len);
	}
	FREE_SPACE(sp, bp, blen);

	vp->m_final = vp->m_stop;
	return (rval);
}

/*
 * v_rcheck --
 *	Check to see if we can do the replacement.
 */
static int
v_rcheck(sp, lno, pp, lenp)
	SCR *sp;
	recno_t lno;
	char **pp;
	size_t *lenp;
{
	/*
	 * If the line doesn't exist, or it's empty, replacement isn't
	 * allowed.  It's not hard to implement, but:
	 *
	 *	1: It's historic practice (vi beeped before the replacement
	 *	   character was even entered).
	 *	2: For consistency, this change would require that the more
	 *	   general case, "Nr", when the user is < N characters from
	 *	   the end of the line, also work, which would be a bit odd.
	 *	3: Replacing with a <newline> has somewhat odd semantics.
	 */
	if ((*pp = file_gline(sp, lno, lenp)) == NULL) {
		FILE_LERR(sp, lno);
		return (1);
	}
	if (*lenp == 0) {
		msgq(sp, M_BERR, "185|No characters to replace");
		return (1);
	}
	return (0);
}
