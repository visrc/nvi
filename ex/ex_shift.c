/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_shift.c,v 5.7 1992/05/04 11:52:06 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:52:06 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "extern.h"

enum which {LEFT, RIGHT};
static void shift __P((EXCMDARG *, enum which));

int
ex_shiftl(cmdp)
	EXCMDARG *cmdp;
{
	shift(cmdp, LEFT);
	return (0);
}
	
int
ex_shiftr(cmdp)
	EXCMDARG *cmdp;
{
	shift(cmdp, RIGHT);
	return (0);
}

static void
shift(cmdp, rl)
	EXCMDARG *cmdp;
	enum which rl;
{
	long	l;	/* line number counter */
	int	oldidx;	/* number of chars previously used for indent */
	int	newidx;	/* number of chars in the new indent string */
	int	oldcol;	/* previous indent amount */
	int	newcol;	/* new indent amount */
	char	*text;	/* pointer to the old line's text */
	char lbuf[2048];

	ChangeText
	{
		/* for each line to shift... */
		for (l = markline(cmdp->addr1); l <= markline(cmdp->addr2); l++)
		{
			/* get the line - ignore empty lines unless ! mode */
			text = fetchline(l, NULL);
			if (!*text && !cmdp->flags & E_FORCE)
				continue;

			/* calc oldidx and oldcol */
			for (oldidx = 0, oldcol = 0;
			     text[oldidx] == ' ' || text[oldidx] == '\t';
			     oldidx++)
			{
				if (text[oldidx] == ' ')
				{
					oldcol += 1;
				}
				else
				{
					oldcol += LVAL(O_TABSTOP) -
					    (oldcol % LVAL(O_TABSTOP));
				}
			}

			/* calc newcol */
			if (rl == RIGHT)
				newcol = oldcol + (LVAL(O_SHIFTWIDTH) & 0xff);
			else {
				newcol = oldcol - (LVAL(O_SHIFTWIDTH) & 0xff);
				if (newcol < 0)
					newcol = 0;
			}

			/* if no change, then skip to next line */
			if (oldcol == newcol)
				continue;

			/* build a new indent string */
			newidx = 0;
			if (ISSET(O_AUTOTAB))
			{
				while (newcol >= LVAL(O_TABSTOP))
				{
					lbuf[newidx++] = '\t';
					newcol -= LVAL(O_TABSTOP);
				}
			}
			while (newcol > 0)
			{
				lbuf[newidx++] = ' ';
				newcol--;
			}
			lbuf[newidx] = '\0';

			/* change the old indent string into the new */
			change(MARK_AT_LINE(l), MARK_AT_LINE(l) + oldidx, lbuf);
		}
	}

	/* Reporting... */
	rptlines = markline(cmdp->addr2) - markline(cmdp->addr1) + 1L;
	rptlabel = rl == RIGHT ? ">ed" : "<ed";

	autoprint = 1;
}
