/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_print.c,v 9.2 1994/11/12 13:10:08 bostic Exp $ (Berkeley) $Date: 1994/11/12 13:10:08 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_list -- :[line [,line]] l[ist] [count] [flags]
 *
 *	Display the addressed lines such that the output is unambiguous.
 */
int
ex_list(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	NEEDFILE(sp, cmdp->cmd);

	if (ex_print(sp, &cmdp->addr1, &cmdp->addr2, cmdp->flags | E_F_LIST))
		return (1);
	sp->lno = cmdp->addr2.lno;
	sp->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_number -- :[line [,line]] nu[mber] [count] [flags]
 *
 *	Display the addressed lines with a leading line number.
 */
int
ex_number(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	NEEDFILE(sp, cmdp->cmd);

	if (ex_print(sp, &cmdp->addr1, &cmdp->addr2, cmdp->flags | E_F_HASH))
		return (1);
	sp->lno = cmdp->addr2.lno;
	sp->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_pr -- :[line [,line]] p[rint] [count] [flags]
 *
 *	Display the addressed lines.
 */
int
ex_pr(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	NEEDFILE(sp, cmdp->cmd);

	if (ex_print(sp, &cmdp->addr1, &cmdp->addr2, cmdp->flags))
		return (1);
	sp->lno = cmdp->addr2.lno;
	sp->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_print --
 *	Print the selected lines.
 */
int
ex_print(sp, fp, tp, flags)
	SCR *sp;
	MARK *fp, *tp;
	register int flags;
{
	recno_t from, to;
	size_t col, len;
	char *p;

	for (from = fp->lno, to = tp->lno; from <= to; ++from) {
		/*
		 * Display the line number.  The %6 format is specified
		 * by POSIX 1003.2, and is almost certainly large enough.
		 * Check, though, just in case.
		 */
		if (LF_ISSET(E_F_HASH))
			if (from <= 999999)
				col = ex_printf(EXCOOKIE, "%6ld  ", from);
			else
				col = ex_printf(EXCOOKIE, "TOOBIG  ");
		else
			col = 0;

		/*
		 * Display the line.  The format for E_F_PRINT isn't very good,
		 * especially in handling end-of-line tabs, but they're almost
		 * backward compatible.
		 */
		if ((p = file_gline(sp, from, &len)) == NULL) {
			GETLINE_ERR(sp, from);
			return (1);
		}

		if (len == 0 && !LF_ISSET(E_F_LIST)) {
			F_SET(sp, S_SCR_EXWROTE);
			(void)ex_printf(EXCOOKIE, "\n");
		} else if (ex_ldisplay(sp, p, len, col, flags))
			return (1);

		if (INTERRUPTED(sp))
			break;
	}
	return (0);
}

/*
 * ex_ldisplay --
 *	Display a line.
 */
int
ex_ldisplay(sp, lp, len, col, flags)
	SCR *sp;
	CHAR_T *lp;
	size_t len, col;
	u_int flags;
{
	CHAR_T ch, *kp;
	u_long ts;
	size_t tlen;

	F_SET(sp, S_SCR_EXWROTE);

	for (ts = O_VAL(sp, O_TABSTOP);; --len) {
		if (len > 0)
			ch = *lp++;
		else if (LF_ISSET(E_F_LIST))
			ch = '$';
		else
			break;
		if (ch == '\t' && !LF_ISSET(E_F_LIST))
			for (tlen = ts - col % ts;
			    col < sp->cols && tlen--; ++col)
				(void)ex_printf(EXCOOKIE, " ");
		else {
			kp = KEY_NAME(sp, ch);
			tlen = KEY_LEN(sp, ch);
			if (col + tlen < sp->cols) {
				(void)ex_printf(EXCOOKIE, "%s", kp);
				col += tlen;
			} else
				for (; tlen--; ++kp, ++col) {
					if (col == sp->cols) {
						col = 0;
						(void)ex_printf(EXCOOKIE, "\n");
					}
					(void)ex_printf(EXCOOKIE, "%c", *kp);
				}
		}
		F_SET(sp, S_SCR_EXWROTE);
		if (INTERRUPTED(sp) || len == 0)
			break;
	}
	if (!INTERRUPTED(sp))
		(void)ex_printf(EXCOOKIE, "\n");
	return (0);
}
