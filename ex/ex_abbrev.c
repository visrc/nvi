/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 5.7 1992/04/16 13:44:53 bostic Exp $ (Berkeley) $Date: 1992/04/16 13:44:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <limits.h>

#include "vi.h"
#include "excmd.h"
#include "seq.h"
#include "extern.h"

int have_abbr;				/* If any abbreviations */

/*
 * ex_abbr -- (:abbreviate [key replacement])
 *	Create an abbreviation or display abbreviations.
 */
int
ex_abbr(cmdp)
	CMDARG *cmdp;
{
	register char *input, *output;

	if (cmdp->string == NULL) {
		seq_dump(ABBREV, 0);
		return (0);
	}

	/*
	 * Abbreviations can't be parsed by the upper-level parser because
	 * input is the first word and output is everything else, i.e. any
	 * space characters are included.
	 */
	for (input = cmdp->string; isspace(*input); ++input);
	for (output = input; *output && !isspace(*output); ++output);
	if (*output != '\0')
		for (*output++ = '\0'; isspace(*output); ++output);
	if (*output == '\0') {
		msg("Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	if (seq_set(NULL, input, output, ABBREV, 1))
		return (1);
	have_abbr = 1;
	return (0);
}

/*
 * ex_unabbr -- (:unabbreviate key)
 *      Delete an abbreviation.
 */
int
ex_unabbr(cmdp)
        CMDARG *cmdp;
{
	char *input;

	input = cmdp->argv[0];
	if (!have_abbr || seq_delete(input, ABBREV)) {
		msg("\"%s\" was never an abbreviation.", input);
		return (1);
	}
	return (0);
}

/*
 * abbr_expand --
 *	
 *	This function is called before each character is inserted.  If we're
 *	at the end of a word, then that word is checked against the set of
 *	abbreviation sequences and expanded, if appropriate.  When returning
 *	from this function, the new character must still be inserted.
 */
MARK
abbr_expand(m, toptr)
	MARK m;			/* The cursor position. */
	MARK *toptr;		/* The end of the text to be changed. */
{
	register SEQ *sp;
	register int len;
	int olen;
	char *word;

	/* See where the preceding word starts. */
	pfetch(markline(m));
	for (word = ptext + markidx(m), len = 0;
	    --word >= ptext && inword(*word); ++len);
	++word;

	/* If zero-length or not in the sequence list, return. */
	if (len == 0 || (sp = seq_find(word, len, ABBREV, NULL)) == NULL)
		return (m);

	/*
	 * Replace the word with the replacement text and return with
	 * the cursor after the end of the replacement text.
	 */
	add(m, sp->output);
	delete(m - len, m);
	olen = strlen(sp->output);
	*toptr = *toptr - len + olen;
	return (m - len + olen);
}

/*
 * abbr_save --
 *	Save the abbreviation sequences to a file.
 */
abbr_save(fp)
	FILE *fp;
{
	return (seq_save(fp, NULL, ABBREV));
}
