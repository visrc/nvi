/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_print.c,v 5.21 1993/02/16 20:10:20 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:10:20 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"

/*
 * ex_list -- :[line [,line]] l[ist] [count] [flags]
 *	Display the addressed lines such that the output is unambiguous.
 *	The only valid flag is '#'.
 */
int
ex_list(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	int flags;

	flags = cmdp->flags & E_F_MASK;
	if (flags & ~E_F_HASH) {
		msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	if (ex_print(ep, &cmdp->addr1, &cmdp->addr2, E_F_LIST | flags))
		return (1);
	ep->lno = cmdp->addr2.lno;
	ep->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_number -- :[line [,line]] nu[mber] [count] [flags]
 *	Display the addressed lines with a leading line number.
 *	The only valid flag is 'l'.
 */
int
ex_number(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	int flags;

	flags = cmdp->flags & E_F_MASK;
	if (flags & ~E_F_LIST) {
		msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	if (ex_print(ep, &cmdp->addr1, &cmdp->addr2, E_F_HASH | flags))
		return (1);
	ep->lno = cmdp->addr2.lno;
	ep->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_pr -- :[line [,line]] p[rint] [count] [flags]
 *	Display the addressed lines.
 *	The only valid flags are '#' and 'l'.
 */
int
ex_pr(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	int flags;

	flags = cmdp->flags & E_F_MASK;
	if (flags & ~(E_F_HASH | E_F_LIST)) {
		msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	if (ex_print(ep, &cmdp->addr1, &cmdp->addr2, E_F_PRINT | flags))
		return (1);
	ep->lno = cmdp->addr2.lno;
	ep->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_print --
 *	Print the selected lines.
 */
int
ex_print(ep, fp, tp, flags)
	EXF *ep;
	MARK *fp, *tp;
	register int flags;
{
	register int ch, col, rlen;
	recno_t from, to;
	size_t len;
	int cnt;
	u_char *p;
	char buf[10];

	for (from = fp->lno, to = tp->lno; from <= to; ++from) {
		/* Display the line number. */
		if (flags & E_F_HASH)
			col = fprintf(ep->stdfp, "%7ld ", from);
		else
			col = 0;
	
		/*
		 * Display the line.  The format for E_F_PRINT isn't very good,
		 * especially in handling end-of-line tabs, but they're almost
		 * backward compatible.
		 */
		if ((p = file_gline(ep, from, &len)) == NULL) {
			GETLINE_ERR(ep, from);
			return (1);
		}

#define	WCHECK(ch) {							\
	if (col == ep->cols) {						\
		(void)putc('\n', ep->stdfp);				\
		col = 0;						\
	}								\
	(void)putc(ch, ep->stdfp);					\
	++col;								\
}
		for (rlen = len; rlen--;) {
			ch = *p++;
			if (flags & E_F_LIST)
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
					while (col < ep->cols &&
					    ++col % LVAL(O_TABSTOP))
						(void)putc(' ', ep->stdfp);
					if (col == ep->cols) {
						col = 0;
						(void)putc('\n', ep->stdfp);
					}
				} else if (isprint(ch)) {
					WCHECK(ch);
				} else if (ch == '\n') {
					col = 0;
					(void)putc('\n', ep->stdfp);
				} else {
					WCHECK('^');
					WCHECK(ch + 0x40);
				}
			}
		}
		if (flags & E_F_LIST)
			WCHECK('$');

		(void)putc('\n', ep->stdfp);
	}
	return (0);
}
