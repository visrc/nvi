/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 8.6 1993/08/19 15:04:33 bostic Exp $ (Berkeley) $Date: 1993/08/19 15:04:33 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

enum which {GLOBAL, VGLOBAL};
static int global __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_global -- [line [,line]] g[lobal][!] /pattern/ [commands]
 *	Exec on lines matching a pattern.
 */
int
ex_global(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (global(sp, ep, cmdp, GLOBAL));
}

/*
 * ex_vglobal -- [line [,line]] v[global] /pattern/ [commands]
 *	Exec on lines not matching a pattern.
 */
int
ex_vglobal(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (global(sp, ep, cmdp, VGLOBAL));
}

static int
global(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	recno_t elno, last1, last2, lno;
	regmatch_t match[1];
	regex_t *re, lre;
	size_t len;
	int delim, eval, reflags, rval;
	char *ptrn, *p, *t, cbuf[1024];

	/*
	 * Skip leading white space.  Historic vi allowed any
	 * non-alphanumeric to serve as the substitution command
	 * delimiter.
	 */
	for (p = cmdp->argv[0]; isspace(*p); ++p);
	delim = *p++;
	if (isalnum(delim)) {
		msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/*
	 * Get the pattern string, toss escaped characters.
	 *
	 * QUOTING NOTE:
	 * Only toss an escaped character if it escapes a delimiter.
	 */
	for (ptrn = t = p;;) {
		if (p[0] == '\0' || p[0] == delim) {
			if (p[0] == delim)
				++p;
			*t = '\0';
			break;
		}
		if (p[0] == '\\' && p[1] == delim)
			++p;
		*t++ = *p++;
	}

	/* Get the command string. */
	if (*p == '\0') {
		msgq(sp, M_ERR, "No command string specified.");
		return (1);
	}

	/* If the substitute string is empty, use the last one. */
	if (*ptrn == '\0') {
		if (!F_ISSET(sp, S_RE_SET)) {
			msgq(sp, M_ERR, "No previous regular expression.");
			return (1);
		}
		re = &sp->sre;
	} else {
		/* Set RE flags. */
		reflags = 0;
		if (O_ISSET(sp, O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (O_ISSET(sp, O_IGNORECASE))
			reflags |= REG_ICASE;

		/* Compile the RE. */
		re = &lre;
		if (eval = regcomp(re, ptrn, reflags)) {
			re_error(sp, eval, re);
			return (1);
		}

		/*
		 * Set saved RE.  Historic practice is that global set
		 * direction as well as the RE.
		 */
		sp->sre = lre;
		sp->searchdir = FORWARD;
		F_SET(sp, S_RE_SET);
	}

	rval = 0;
	F_CLR(sp, S_INTERRUPTED);
	F_SET(sp, S_GLOBAL | S_INTERRUPTIBLE);

	/* For each line... */
	for (lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {

		/* Get the line. */
		if ((t = file_gline(sp, ep, lno, &len)) == NULL) {
			GETLINE_ERR(sp, lno);
			rval = 1;
			goto quit;
		}

		/* Search for a match. */
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		switch(eval = regexec(re, t, 1, match, REG_STARTEND)) {
		case 0:
			if (cmd == VGLOBAL)
				continue;
			break;
		case REG_NOMATCH:
			if (cmd == GLOBAL)
				continue;
			break;
		default:
			re_error(sp, eval, re);
			rval = 1;
			goto quit;
		}

		/*
		 * Execute the command, keeping track of the lines that
		 * change, and adjusting for inserted/deleted lines.
		 */
		if (file_lline(sp, ep, &last1))
			goto quit;

		sp->lno = lno;
		(void)snprintf(cbuf, sizeof(cbuf), "%s", p);
		if (ex_cmd(sp, ep, cbuf, 0)) {
			rval = 1;
			goto quit;
		}

		/* Someone's unhappy, time to stop. */
		if (F_ISSET(sp, S_INTERRUPTED))
			goto quit;

		if (file_lline(sp, ep, &last2))
			goto quit;
		if (last2 > last1) {
			last2 -= last1;
			lno += last2;
			elno += last2;
		} else if (last1 > last2) {
			last1 -= last2;
			lno -= last1;
			elno -= last1;
		}

		/* Cursor moves to last line sent to command. */
		sp->lno = lno;
	}

quit:	F_CLR(sp, S_GLOBAL | S_INTERRUPTED | S_INTERRUPTIBLE);
	return (rval);
}
