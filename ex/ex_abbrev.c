/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 5.16 1993/02/28 14:00:26 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:26 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "seq.h"

int have_abbr;				/* If any abbreviations */

/*
 * ex_abbr -- :abbreviate [key replacement]
 *	Create an abbreviation or display abbreviations.
 */
int
ex_abbr(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	register u_char *input, *output;

	if (cmdp->string == NULL) {
		if (seq_dump(ep, ABBREV, 0) == 0)
			ep->msg(ep, M_ERROR, "No abbreviations.");
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
		ep->msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	if (seq_set(ep, NULL, input, output, ABBREV, 1))
		return (1);
	have_abbr = 1;
	return (0);
}

/*
 * ex_unabbr -- :unabbreviate key
 *      Delete an abbreviation.
 */
int
ex_unabbr(ep, cmdp)
	EXF *ep;
        EXCMDARG *cmdp;
{
	u_char *input;

	input = cmdp->argv[0];
	if (!have_abbr || seq_delete(input, ABBREV)) {
		ep->msg(ep, M_ERROR,
		    "\"%s\" was never an abbreviation.", input);
		return (1);
	}
	return (0);
}

/*
 * abbr_save --
 *	Save the abbreviation sequences to a file.
 */
int
abbr_save(fp)
	FILE *fp;
{
	return (seq_save(fp, NULL, ABBREV));
}
