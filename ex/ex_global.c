/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 5.31 1993/05/16 20:18:10 bostic Exp $ (Berkeley) $Date: 1993/05/16 20:18:10 $";
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
	int eval, reflags, rval;
	char *endp, *ptrn, *s, cbuf[512];
	char delim[2];

	/* Skip whitespace. */
	for (s = cmdp->string; *s && isspace(*s); ++s);

	/* Get delimiter. */
	if (*s != '/' && *s != ';') {
		msgq(sp, M_ERR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* Delimiter is the first character. */
	delim[0] = s[0];
	delim[1] = '\0';

	/* Get the pattern string. */
	endp = s + 1;
	ptrn = strsep(&endp, delim);

	/* Get the command string. */
	if (endp == NULL || *endp == NULL) {
		msgq(sp, M_ERR, "No command string specified.");
		return (1);
	}

	/* If the substitute string is empty, use the last one. */
	if (*ptrn == NULL) {
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
		if (eval = regcomp(re, (char *)ptrn, reflags)) {
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
	F_SET(sp, S_GLOBAL);
	F_CLR(sp, S_GLOBAL_QUIT);

	/* For each line... */
	for (lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {

		/* Get the line. */
		if ((s = file_gline(sp, ep, lno, &len)) == NULL) {
			GETLINE_ERR(sp, lno);
			rval = 1;
			goto quit;
		}

		/* Search for a match. */
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		switch(eval = regexec(re, (char *)s, 1, match, REG_STARTEND)) {
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
		(void)snprintf((char *)cbuf, sizeof(cbuf), "%s", endp);
		if (ex_cmd(sp, ep, cbuf)) {
			rval = 1;
			goto quit;
		}

		/* Someone's unhappy, time to stop. */
		if (F_ISSET(sp, S_GLOBAL_QUIT))
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

quit:	F_CLR(sp, S_GLOBAL | S_GLOBAL_QUIT);
	return (rval);
}
