/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 5.1 1992/05/04 11:55:45 bostic Exp $ (Berkeley) $Date: 1992/05/04 11:55:45 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"
#include "regexp.h"
#include "extern.h"

static regexp *re;		/* Compiled version of the pattern. */
enum direction searchdir;

MARK
f_search(m, ptrn)
	MARK	m;	/* where to start searching */
	char	*ptrn;	/* pattern to search for */
{
	long	l;	/* line# of line to be searched */
	char	*line;	/* text of line to be searched */
	int	wrapped;/* boolean: has our search wrapped yet? */
	int	pos;	/* where we are in the line */
	long	delta = INFINITY;/* line offset, for things like "/foo/+1" */

	searchdir = FORWARD;
	if (ptrn && *ptrn)
	{
		/* locate the closing '/', if any */
		line = parseptrn(ptrn);
		if (*line)
		{
			delta = atol(line);
		}
		ptrn++;

		/* free the previous pattern */
		if (re) free(re);

		/* compile the pattern */
		re = regcomp(ptrn);
		if (!re)
		{
			return MARK_UNSET;
		}
	}
	else if (!re)
	{
		msg("No previous expression");
		return MARK_UNSET;
	}

	/* search forward for the pattern */
	pos = markidx(m) + 1;
	pfetch(markline(m));
	if (pos >= plen)
	{
		pos = 0;
		m = (m | (BLKSIZE - 1)) + 1;
	}
	wrapped = FALSE;
	for (l = markline(m); l != markline(m) + 1 || !wrapped; l++)
	{
		/* wrap search */
		if (l > nlines)
		{
			/* if we wrapped once already, then the search failed */
			if (wrapped)
			{
				break;
			}

			/* else maybe we should wrap now? */
			if (ISSET(O_WRAPSCAN))
			{
				l = 0;
				wrapped = TRUE;
				continue;
			}
			else
			{
				break;
			}
		}

		/* get this line */
		line = fetchline(l, NULL);

		/* check this line */
		if (regexec(re, &line[pos], (pos == 0)))
		{
			/* match! */
			if (wrapped && ISSET(O_WARN))
				msg("(wrapped)");
			if (delta != INFINITY)
			{
				l += delta;
				if (l < 1 || l > nlines)
				{
					msg("search offset too big");
					return MARK_UNSET;
				}
				force_lnmd = TRUE;
				return MARK_AT_LINE(l);
			}
			return MARK_AT_LINE(l) + (int)(re->startp[0] - line);
		}
		pos = 0;
	}

	/* not found */
	msg(ISSET(O_WRAPSCAN) ? "Not found" : "Hit bottom without finding RE");
	return MARK_UNSET;
}

MARK
b_search(m, ptrn)
	MARK	m;	/* where to start searching */
	char	*ptrn;	/* pattern to search for */
{
	long	l;	/* line# of line to be searched */
	char	*line;	/* text of line to be searched */
	int	wrapped;/* boolean: has our search wrapped yet? */
	int	pos;	/* last acceptable idx for a match on this line */
	int	last;	/* remembered idx of the last acceptable match on this line */
	int	try;	/* an idx at which we strat searching for another match */
	long	delta = INFINITY;/* line offset, for things like "/foo/+1" */

	searchdir = BACKWARD;
	if (ptrn && *ptrn)
	{
		/* locate the closing '?', if any */
		line = parseptrn(ptrn);
		if (*line)
		{
			delta = atol(line);
		}
		ptrn++;

		/* free the previous pattern, if any */
		if (re) free(re);

		/* compile the pattern */
		re = regcomp(ptrn);
		if (!re)
		{
			return MARK_UNSET;
		}
	}
	else if (!re)
	{
		msg("No previous expression");
		return MARK_UNSET;
	}

	/* search backward for the pattern */
	pos = markidx(m);
	wrapped = FALSE;
	for (l = markline(m); l != markline(m) - 1 || !wrapped; l--)
	{
		/* wrap search */
		if (l < 1)
		{
			if (ISSET(O_WRAPSCAN))
			{
				l = nlines + 1;
				wrapped = TRUE;
				continue;
			}
			else
			{
				break;
			}
		}

		/* get this line */
		line = fetchline(l, NULL);

		/* check this line */
		if (regexec(re, line, 1) && (int)(re->startp[0] - line) < pos)
		{
			/* match!  now find the last acceptable one in this line */
			do
			{
				last = (int)(re->startp[0] - line);
				try = (int)(re->endp[0] - line);
			} while (try > 0
				 && regexec(re, &line[try], FALSE)
				 && (int)(re->startp[0] - line) < pos);

			if (wrapped && ISSET(O_WARN))
				msg("(wrapped)");
			if (delta != INFINITY)
			{
				l += delta;
				if (l < 1 || l > nlines)
				{
					msg("search offset too big");
					return MARK_UNSET;
				}
				force_lnmd = TRUE;
				return MARK_AT_LINE(l);
			}
			return MARK_AT_LINE(l) + last;
		}
		pos = BLKSIZE;
	}

	/* not found */
	msg(ISSET(O_WRAPSCAN) ? "Not found" : "Hit top without finding RE");
	return MARK_UNSET;
}
