/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_print.c,v 5.15 1992/10/17 15:20:31 bostic Exp $ (Berkeley) $Date: 1992/10/17 15:20:31 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "term.h"
#include "extern.h"

static int print __P((EXCMDARG *, int));

/*
 * ex_list -- :[line [,line]] l[ist] [count] [flags]
 *	Display the addressed lines such that the output is unambiguous.
 *	The only valid flag is '#'.
 */
int
ex_list(cmdp)
	EXCMDARG *cmdp;
{
	int flags;

	flags = cmdp->flags & E_F_MASK;
	if (flags & ~E_F_HASH) {
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	return (print(cmdp, E_F_LIST | flags));
}

/*
 * ex_number -- :[line [,line]] nu[mber] [count] [flags]
 *	Display the addressed lines with a leading line number.
 *	The only valid flag is 'l'.
 */
int
ex_number(cmdp)
	EXCMDARG *cmdp;
{
	int flags;

	flags = cmdp->flags & E_F_MASK;
	if (flags & ~E_F_LIST) {
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	return (print(cmdp, E_F_HASH | flags));
}

/*
 * ex_print -- :[line [,line]] p[rint] [count] [flags]
 *	Display the addressed lines.
 *	The only valid flags are '#' and 'l'.
 */
int
ex_print(cmdp)
	EXCMDARG *cmdp;
{
	int flags;

	flags = cmdp->flags & E_F_MASK;
	if (flags & ~(E_F_HASH | E_F_LIST)) {
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}
	return (print(cmdp, E_F_PRINT | flags));
}

/*
 * print --
 *	Print the selected lines.
 */
static int
print(cmdp, flags)
	EXCMDARG *cmdp;
	register int flags;
{
	register recno_t cur, end;
	register int ch, col, rlen;
	size_t len;
	int cnt;
	u_char *p;
	char buf[10];

	EX_PRSTART(0);
	for (cur = cmdp->addr1.lno, end = cmdp->addr2.lno; cur <= end; ++cur) {

		/* Display the line number. */
		if (flags & E_F_HASH) {
			(void)printf("%7ld ", cur);
			col = 8;
		} else
			col = 0;
	
#define	WCHECK(ch) { \
	if (col == COLS) { \
		EX_PRNEWLINE; \
		col = 0; \
	} \
	(void)putchar(ch); \
	++col; \
}
		/*
		 * Display the line.  The format for E_F_PRINT isn't very good,
		 * especially in handling end-of-line tabs, but they're almost
		 * backward compatible.
		 */
		p = file_gline(curf, cur, &len);
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
					while (col < COLS &&
					    ++col % LVAL(O_TABSTOP))
						(void)putchar(' ');
					if (col == COLS) {
						col = 0;
						EX_PRNEWLINE;
					}
				} else if (isprint(ch)) {
					WCHECK(ch);
				} else if (ch == '\n') {
					col = 0;
					EX_PRNEWLINE;
				} else {
					WCHECK('^');
					WCHECK(ch + 0x40);
				}
			}
		}
		if (flags & E_F_LIST)
			WCHECK('$');

		/* The print commands require a keystroke to continue. */
		EX_PRNEWLINE;
	}
	curf->lno = cmdp->addr2.lno;
	curf->cno = cmdp->addr2.cno;
	
	return (0);
}
