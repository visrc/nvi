/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 9.2 1995/01/11 16:15:14 bostic Exp $ (Berkeley) $Date: 1995/01/11 16:15:14 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../vi/vcmd.h"

/*
 * ex_abbr -- :abbreviate [key replacement]
 *	Create an abbreviation or display abbreviations.
 */
int
ex_abbr(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	CHAR_T *p;
	size_t len;

	switch (cmdp->argc) {
	case 0:
		if (seq_dump(sp, SEQ_ABBREV, 0) == 0)
			msgq(sp, M_INFO, "233|No abbreviations to display");
		return (0);
	case 2:
		break;
	default:
		abort();
	}

	/*
	 * Check for illegal characters.
	 *
	 * !!!
	 * Another fun one, historically.  See vi/v_ntext.c:txt_abbrev() for
	 * details.  The bottom line is that all abbreviations have to end
	 * with a "word" character, because it's the transition from word to
	 * non-word characters that triggers the test for an abbreviation.  In
	 * addition, because of the way the test is done, there can't be any
	 * transitions from word to non-word character (or vice-versa) other
	 * than between the next-to-last and last characters of the string,
	 * and there can't be any <blank> characters.
	 * Warn the user.
	 */
	if (!inword(cmdp->argv[0]->bp[cmdp->argv[0]->len - 1])) {
		msgq(sp, M_ERR,
		    "234|Abbreviations must end with a \"word\" character");
			return (1);
	}
	for (p = cmdp->argv[0]->bp; *p != '\0'; ++p)
		if (isblank(p[0])) {
			msgq(sp, M_ERR,
			    "235|Abbreviations may not contain tabs or spaces");
			return (1);
		}
	if (cmdp->argv[0]->len > 2)
		for (p = cmdp->argv[0]->bp,
		    len = cmdp->argv[0]->len - 2; len; --len, ++p)
			if (inword(p[0]) != inword(p[1])) {
				msgq(sp, M_ERR,
"236|Abbreviations may not mix word/non-word characters, except at the end");
				return (1);
			}

	if (seq_set(sp, NULL, 0, cmdp->argv[0]->bp, cmdp->argv[0]->len,
	    cmdp->argv[1]->bp, cmdp->argv[1]->len, SEQ_ABBREV, SEQ_USERDEF))
		return (1);

	F_SET(sp->gp, G_ABBREV);
	return (0);
}

/*
 * ex_unabbr -- :unabbreviate key
 *      Delete an abbreviation.
 */
int
ex_unabbr(sp, cmdp)
	SCR *sp;
        EXCMDARG *cmdp;
{
	ARGS *ap;
	int nf;
	char *p;

	ap = cmdp->argv[0];
	if (!F_ISSET(sp->gp, G_ABBREV) ||
	    seq_delete(sp, ap->bp, ap->len, SEQ_ABBREV)) {
		p = msg_print(sp, ap->bp, &nf);
		msgq(sp, M_ERR, "237|\"%s\" is not an abbreviation", ap->bp);
		if (nf)
			FREE_SPACE(sp, p, 0);
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
	return (seq_save(sp, fp, "abbreviate ", SEQ_ABBREV));
}
