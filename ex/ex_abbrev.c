/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 5.11 1992/05/07 12:46:24 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:46:24 $";
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
 * ex_abbr -- :abbreviate [key replacement]
 *	Create an abbreviation or display abbreviations.
 */
int
ex_abbr(cmdp)
	EXCMDARG *cmdp;
{
	register char *input, *output;

	if (cmdp->string == NULL) {
		if (seq_dump(ABBREV, 0) == 0)
			msg("No abbreviations.");
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
 * ex_unabbr -- :unabbreviate key
 *      Delete an abbreviation.
 */
int
ex_unabbr(cmdp)
        EXCMDARG *cmdp;
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
 * abbr_save --
 *	Save the abbreviation sequences to a file.
 */
abbr_save(fp)
	FILE *fp;
{
	return (seq_save(fp, NULL, ABBREV));
}
