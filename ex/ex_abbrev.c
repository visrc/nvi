/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 5.2 1992/04/04 10:02:25 bostic Exp $ (Berkeley) $Date: 1992/04/04 10:02:25 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

static struct _AB {
	struct _AB *next;
	char *large;		/* the expanded form */
	char small[1];	/* the abbreviated form (appended to struct) */
} *abbrev;

void
ex_abbr(cmdp)
	CMDARG *cmdp;
{
	char	*extra;
	int		smlen;	/* length of the small form */
	int		lrg;	/* index of the start of the large form */
	REG struct _AB	*ab;	/* used to move through the abbrev list */
	struct _AB	*prev;

	/* no arguments? */
	extra = cmdp->argv[0];
	if (!*extra)
	{
		/* list all current abbreviations */
		for (ab = abbrev; ab; ab = ab->next)
		{
			qaddstr("abbr ");
			qaddstr(ab->small);
			qaddch(' ');
			qaddstr(ab->large);
			addch('\n');
			exrefresh();
		}
		return;
	}

	/* else one or more arguments.  Parse the first & look up in abbrev[] */
	for (smlen = 0; extra[smlen] && isalnum(extra[smlen]); smlen++)
	{
	}
	for (prev = (struct _AB *)0, ab = abbrev; ab; prev = ab, ab = ab->next)
	{
		if (!strncmp(extra, ab->small, smlen) && !ab->small[smlen])
		{
			break;
		}
	}

	/* locate the start of the large form, if any */
	for (lrg = smlen; extra[lrg] && isspace(extra[lrg]); lrg++)
	{
	}

	/* only one arg? */
	if (!extra[lrg])
	{
		/* trying to undo an abbreviation which doesn't exist? */
		if (!ab)
		{
			msg("\"%s\" not an abbreviation", extra);
			return;
		}

		/* undo the abbreviation */
		if (prev)
			prev->next = ab->next;
		else
			abbrev = ab->next;
		free(ab->large);
		free(ab);

		return;
	}

	/* multiple args - [re]define an abbreviation */
	if (ab)
	{
		/* redefining - free the old large form */
		free(ab->large);
	}
	else
	{
		/* adding a new definition - make a new struct */
		ab = (struct _AB *)malloc((unsigned)(smlen + sizeof *ab));
		if (!ab)
		{
			msg("Out of memory -- Sorry");
			return;
		}
		strncpy(ab->small, extra, smlen);
		ab->small[smlen] = '\0';
		ab->next = (struct _AB *)0;
		if (prev)
			prev->next = ab;
		else
			abbrev = ab;
	}

	/* store the new form */
	ab->large = (char *)malloc((unsigned)(strlen(&extra[lrg]) + 1));
	strcpy(ab->large, &extra[lrg]);
}

/* This function is called from ex_mkexrc() to save the abbreviations */
void
abbr_save(fd)
	int	fd;	/* fd to which the :abbr commands should be written */
{
	REG struct _AB	*ab;

	for (ab = abbrev; ab; ab = ab->next)
	{
		write(fd, "abbr ", 5);
		write(fd, ab->small, strlen(ab->small));
		write(fd, " ", 1);
		write(fd, ab->large, strlen(ab->large));
		write(fd, "\n", 1);
	}
}

/* This function should be called before each char is inserted.  If the next
 * char is non-alphanumeric and we're at the end of a word, then that word
 * is checked against the abbrev[] array and expanded, if appropriate.  Upon
 * returning from this function, the new char still must be inserted.
 */
MARK
abbr_expand(m, ch, toptr)
	MARK		m;	/* the cursor position */
	int		ch;	/* the character to insert */
	MARK		*toptr;	/* the end of the text to be changed */
{
	char		*word;	/* where the word starts */
	int		len;	/* length of the word */
	REG struct _AB	*ab;

	/* if no abbreviations are in effect, or ch is aphanumeric, then
	 * don't do anything
	 */
	if (!abbrev || isalnum(ch))
	{
		return m;
	}

	/* see where the preceding word starts */
	pfetch(markline(m));
	for (word = ptext + markidx(m), len = 0;
	     --word >= ptext && isalnum(*word);
	     len++)
	{
	}
	word++;

	/* if zero-length, then it isn't a word, really -- so nothing */
	if (len == 0)
	{
		return m;
	}

	/* look it up in the abbrev list */
	for (ab = abbrev; ab; ab = ab->next)
	{
		if (!strncmp(ab->small, word, len) && !ab->small[len])
		{
			break;
		}
	}

	/* not an abbreviation? then do nothing */
	if (!ab)
	{
		return m;
	}

	/* else replace the small form with the large form */
	add(m, ab->large);
	delete(m - len, m);

	/* return with the cursor after the end of the large form */
	*toptr = *toptr - len + strlen(ab->large);
	return m - len + strlen(ab->large);
}
