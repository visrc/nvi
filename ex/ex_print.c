/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_print.c,v 5.2 1992/04/04 10:02:40 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "options.h"
#include "vi.h"
#include "excmd.h"
#include "extern.h"

enum which {LIST, NUMBER, PRINT};
static void print __P((CMDARG *, enum which));

/*
 * ex_list -- (:[line [,line]] l[ist] [count] [flags])
 *	Write the addressed lines such that the output is unambiguous.  The
 *	only valid flag is '#'.
 */
void
ex_list(cmdp)
	CMDARG *cmdp;
{
	print(cmdp, LIST);
}

/*
 * ex_number -- (:[line [,line]] nu[mber] [count] [flags])
 *	Write the addressed lines with a leading line number.  The only valid
 *	flag is 'l'.
 */
void
ex_number(cmdp)
	CMDARG *cmdp;
{
	print(cmdp, NUMBER);
}

/*
 * ex_print -- (:[line [,line]] p[rint] [count] [flags])
 *	Write the addressed lines.  The only meaningful flags are '#' and 'l'.
 */
void
ex_print(cmdp)
	CMDARG *cmdp;
{
	print(cmdp, PRINT);
}

/* print the selected lines */
static void
print(cmdp, cmd)
	CMDARG *cmdp;
	enum which cmd;
{
	REG char	*scan;
	REG long	l;
	REG int		col;

	for (l = markline(cmdp->addr1); l <= markline(cmdp->addr2); l++)
	{
		/* display a line number, if CMD_NUMBER */
		if (cmd == NUMBER)
		{
			sprintf(tmpblk.c, "%6ld  ", l);
			qaddstr(tmpblk.c);
			col = 8;
		}
		else
		{
			col = 0;
		}

		/* get the next line & display it */
		for (scan = fetchline(l); *scan; scan++)
		{
			/* expand tabs to the proper width */
			if (*scan == '\t' && cmd != LIST)
			{
				do
				{
					qaddch(' ');
					col++;
				} while (col % LVAL(O_TABSTOP) != 0);
			}
			else if (*scan > 0 && *scan < ' ' || *scan == '\177')
			{
				qaddch('^');
				qaddch(*scan ^ 0x40);
				col += 2;
			}
			else if ((*scan & 0x80) && cmd == LIST)
			{
				sprintf(tmpblk.c, "\\%03o", *scan);
				qaddstr(tmpblk.c);
				col += 4;
			}
			else
			{
				qaddch(*scan);
				col++;
			}

			/* wrap at the edge of the screen */
			if (!has_AM && col >= COLS)
			{
				addch('\n');
				col -= COLS;
			}
		}
		if (cmd == LIST)
		{
			qaddch('$');
		}
		addch('\n');
		exrefresh();
	}
}
