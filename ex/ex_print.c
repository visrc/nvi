/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_print.c,v 8.5 1993/09/09 14:01:44 bostic Exp $ (Berkeley) $Date: 1993/09/09 14:01:44 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_list -- :[line [,line]] l[ist] [count] [flags]
 *
 *	Display the addressed lines such that the output is unambiguous.
 */
int
ex_list(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (ex_print(sp, ep,
	    &cmdp->addr1, &cmdp->addr2, cmdp->flags | E_F_LIST))
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
ex_number(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (ex_print(sp, ep,
	    &cmdp->addr1, &cmdp->addr2, cmdp->flags | E_F_HASH))
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
ex_pr(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	if (ex_print(sp, ep, &cmdp->addr1, &cmdp->addr2, cmdp->flags))
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
ex_print(sp, ep, fp, tp, flags)
	SCR *sp;
	EXF *ep;
	MARK *fp, *tp;
	register int flags;
{
	register int ch, col, rlen;
	recno_t from, to;
	size_t len;
	int cnt;
	char *p, buf[10];

	F_SET(sp, S_INTERRUPTIBLE);
	for (from = fp->lno, to = tp->lno; from <= to; ++from) {
		/* Display the line number. */
		if (LF_ISSET(E_F_HASH))
			col = fprintf(sp->stdfp, "%7ld ", from);
		else
			col = 0;
	
		/*
		 * Display the line.  The format for E_F_PRINT isn't very good,
		 * especially in handling end-of-line tabs, but they're almost
		 * backward compatible.
		 */
		if ((p = file_gline(sp, ep, from, &len)) == NULL) {
			GETLINE_ERR(sp, from);
			return (1);
		}

#define	WCHECK(ch) {							\
	if (col == sp->cols) {						\
		(void)putc('\n', sp->stdfp);				\
		col = 0;						\
	}								\
	(void)putc(ch, sp->stdfp);					\
	++col;								\
}
		for (rlen = len; rlen--;) {
			ch = *p++;
			if (LF_ISSET(E_F_LIST))
				if (ch != '\t' && isprint(ch)) {
					WCHECK(ch);
				} else if (ch & 0x80) {
					(void)snprintf(buf,
					    sizeof(buf), "\\%03o", ch);
					len = strlen(buf);
					for (cnt = 0; cnt < len; ++cnt)
						WCHECK(buf[cnt]);
				} else {
					WCHECK('^');
					WCHECK(ch + 0x40);
				}
			else {
				ch &= 0x7f;
				if (ch == '\t') {
					while (col < sp->cols &&
					    ++col % O_VAL(sp, O_TABSTOP))
						(void)putc(' ', sp->stdfp);
					if (col == sp->cols) {
						col = 0;
						(void)putc('\n', sp->stdfp);
					}
				} else if (isprint(ch)) {
					WCHECK(ch);
				} else if (ch == '\n') {
					col = 0;
					(void)putc('\n', sp->stdfp);
				} else {
					WCHECK('^');
					WCHECK(ch + 0x40);
				}
			}
		}
		if (LF_ISSET(E_F_LIST)) {
			WCHECK('$');
		} else if (len == 0) {
			/*
			 * If the line is empty, output a space
			 * to overwrite the colon prompt.
			 */
			WCHECK(' ');
		}
		(void)putc('\n', sp->stdfp);

		if (F_ISSET(sp, S_INTERRUPTED))
			break;
	}
	return (0);
}
