/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: delete.c,v 5.3 1992/05/07 12:45:24 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:45:24 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "extern.h"

#ifdef NOTDEF
/* delete a range of text from the file */
void delete(frommark, tomark)
	MARK		frommark;	/* first char to be deleted */
	MARK		tomark;		/* AFTER last char to be deleted */
{
	int		i;		/* used to move thru logical blocks */
	REG char	*scan;		/* used to scan thru text of the blk */
	REG char	*cpy;		/* used when copying chars */
	BLK		*blk;		/* a text block */
	long		l;		/* a line number */
	MARK		m;		/* a traveling version of frommark */

#ifdef DEBUG
	TRACE("delete (%ld.%d, %ld.%d)\n", markline(frommark),
	    markidx(frommark), markline(tomark), markidx(tomark));
#endif

	/* if not deleting anything, quit now */
	if (frommark == tomark)
	{
		return;
	}

	/* This is a change */
	changes++;
	setflag(file, MODIFIED);

	/* adjust marks 'a through 'z and '' as needed */
	l = markline(tomark);
	for (i = 0; i < NMARKS; i++)
	{
		if (mark[i] < frommark)
		{
			continue;
		}
		else if (mark[i] < tomark)
		{
			mark[i] = MARK_UNSET;
		}
		else if (markline(mark[i]) == l)
		{
			if (markline(frommark) == l)
			{
				mark[i] -= markidx(tomark) - markidx(frommark);
			}
			else
			{
				mark[i] -= markidx(tomark);
			}
		}
		else
		{
			mark[i] -= MARK_AT_LINE(l - markline(frommark));
		}
	}

	/* Reporting... */
	if (markidx(frommark) == 0 && markidx(tomark) == 0)
	{
		rptlines = markline(tomark) - markline(frommark);
		rptlabel = "deleted";
	}

	/* find the block containing frommark */
	l = markline(frommark);
	for (i = 1; lnum[i] < l; i++)
	{
	}

	/* process each affected block... */
	for (m = frommark;
	     m < tomark && lnum[i] < INFINITY;
	     m = MARK_AT_LINE(lnum[i - 1] + 1))
	{
		/* fetch the block */
		blk = blkget(i);

		/* find the mark in the block */
		scan = blk->c;
		for (l = markline(m) - lnum[i - 1] - 1; l > 0; l--)
		{
			while (*scan++ != '\n')
			{
			}
		}
		scan += markidx(m);

		/* figure out where the changes to this block end */
		if (markline(tomark) > lnum[i])
		{
			cpy = blk->c + BLKSIZE;
		}
		else if (markline(tomark) == markline(m))
		{
			cpy = scan - markidx(m) + markidx(tomark);
		}
		else
		{
			cpy = scan;
			for (l = markline(tomark) - markline(m);
			     l > 0;
			     l--)
			{
				while (*cpy++ != '\n')
				{
				}
			}
			cpy += markidx(tomark);
		}

		/* delete the stuff by moving chars within this block */
		while (cpy < blk->c + BLKSIZE)
		{
			*scan++ = *cpy++;
		}
		while (scan < blk->c + BLKSIZE)
		{
			*scan++ = '\0';
		}

		/* adjust tomark to allow for lines deleted from this block */
		tomark -= MARK_AT_LINE(lnum[i] + 1 - markline(m));

		/* if this block isn't empty now, then advance i */
		if (*blk->c)
		{
			i++;
		}

		/* the buffer has changed.  Update hdr and lnum. */
		blkdirty(blk);
	}

	/* must have at least 1 line */
	if (nlines == 0)
	{
		blk = blkadd(1);
		blk->c[0] = '\n';
		blkdirty(blk);
		cursor = MARK_FIRST;
	}
}
#else
int
delete(fm, tm)
	MARK *fm, *tm;
{
	return (0);
}
#endif
