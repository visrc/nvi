/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 8.3 1993/11/27 15:52:30 bostic Exp $ (Berkeley) $Date: 1993/11/27 15:52:30 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>

#include "vi.h"
#include "seq.h"
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
	char *input, *output;

	switch (cmdp->argc) {
	case 0:
		if (seq_dump(sp, SEQ_ABBREV, 0) == 0)
			msgq(sp, M_INFO, "No abbreviations.");
		return (0);
	case 2:
		input = cmdp->argv[0];
		output = cmdp->argv[1];
		break;
	default:
		abort();
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
	char *input;

	input = cmdp->argv[0];
	if (!F_ISSET(sp, S_ABBREV) || seq_delete(sp, input, SEQ_ABBREV)) {
		msgq(sp, M_ERR,
		    "\"%s\" is not an abbreviation.", input);
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
