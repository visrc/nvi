/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 5.18 1993/03/26 13:38:40 bostic Exp $ (Berkeley) $Date: 1993/03/26 13:38:40 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_abbr -- :abbreviate [key replacement]
 *	Create an abbreviation or display abbreviations.
 */
int
ex_abbr(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	register u_char *input, *output;

	if (cmdp->string == NULL) {
		if (seq_dump(sp, SEQ_ABBREV, 0) == 0)
			msgq(sp, M_ERROR, "No abbreviations.");
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
		msgq(sp, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	if (seq_set(sp, NULL, input, output, SEQ_ABBREV, 1))
		return (1);
	F_SET(sp, S_ABBREV);
	return (0);
}

/*
 * ex_unabbr -- :unabbreviate key
 *      Delete an abbreviation.
 */
int
ex_unabbr(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
        EXCMDARG *cmdp;
{
	u_char *input;

	input = cmdp->argv[0];
	if (!F_ISSET(sp, S_ABBREV) || seq_delete(sp, input, SEQ_ABBREV)) {
		msgq(sp, M_ERROR,
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
abbr_save(sp, fp)
	SCR *sp;
	FILE *fp;
{
	return (seq_save(sp, fp, NULL, SEQ_ABBREV));
}
