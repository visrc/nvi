/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_abbrev.c,v 8.15 1994/08/29 09:56:31 bostic Exp $ (Berkeley) $Date: 1994/08/29 09:56:31 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
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
ex_abbr(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CHAR_T *p;
	size_t len;

	switch (cmdp->argc) {
	case 0:
		if (seq_dump(sp, SEQ_ABBREV, 0) == 0)
			msgq(sp, M_INFO, "No abbreviations to display");
		return (0);
	case 2:
		break;
	default:
		abort();
	}

	/*
	 * Check for illegal characters.
	 *
	 * XXX
	 * Another fun one.  Historically, vi permitted the user to enter any
	 * set of characters as abbreviations.  So, all of:
	 *
	 *	:abbr  +++i  __+++i__
	 *	:abbr  i+++  __i+++__
	 *	:abbr  ++++  __++++__
	 *
	 * were legal ex commands.  The problem arises because the text input
	 * code which checked for an abbreviation match checked from non-word
	 * characters to the previous whitespace character.  This means that
	 * the first example above can be found, but the second and third can't.
	 * Warn the user.
	 */
	for (p = cmdp->argv[0]->bp, len = cmdp->argv[0]->len; len; ++p, len--)
		if (inword(*p))
			break;
	if (len == 0) {
		msgq(sp, M_ERR,
		    "Abbreviations must contiain a least one word character");
			return (1);
	}
	for (; len; ++p, len--)
		if (!inword(*p)) {
			msgq(sp, M_ERR,
		    "%s may not follow word characters in an abbreviated word",
			    KEY_NAME(sp, *p));
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
ex_unabbr(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
        EXCMDARG *cmdp;
{
	ARGS *ap;

	ap = cmdp->argv[0];
	if (!F_ISSET(sp->gp, G_ABBREV) ||
	    seq_delete(sp, ap->bp, ap->len, SEQ_ABBREV)) {
		msgq(sp, M_ERR, "\"%s\" is not an abbreviation", ap->bp);
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
