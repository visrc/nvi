/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 5.21 1993/02/28 14:00:35 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:35 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "screen.h"

enum which {GLOBAL, VGLOBAL};
static int global __P((EXF *, EXCMDARG *, enum which));

/*
 * ex_global -- [line [,line]] g[lobal][!] /pattern/ [commands]
 *	Exec on lines matching a pattern.
 */
int
ex_global(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (global(ep, cmdp, GLOBAL));
}

/*
 * ex_vglobal -- [line [,line]] v[global] /pattern/ [commands]
 *	Exec on lines not matching a pattern.
 */
int
ex_vglobal(ep, cmdp)
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (global(ep, cmdp, VGLOBAL));
}

static int
global(ep, cmdp, cmd)
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	recno_t elno, last1, last2, lno, nchanged;
	regmatch_t match[1];
	regex_t *re, lre;
	size_t len;
	int eval, reflags, rval;
	u_char *endp, *ptrn, *s, cbuf[512];
	char delim[2];

	/* Skip whitespace. */
	for (s = cmdp->string; *s && isspace(*s); ++s);

	/* Get delimiter. */
	if (*s != '/' && *s != ';') {
		ep->msg(ep, M_ERROR, "Usage: %s.", cmdp->cmd->usage);
		return (1);
	}

	/* Delimiter is the first character. */
	delim[0] = s[0];
	delim[1] = '\0';

	/* Get the pattern string. */
	endp = s + 1;
	ptrn = USTRSEP(&endp, delim);

	/* Get the command string. */
	if (*endp == NULL) {
		ep->msg(ep, M_ERROR, "No command string specified.");
		return (1);
	}

	/* If the substitute string is empty, use the last one. */
	if (*ptrn == NULL) {
		if (!FF_ISSET(ep, F_RE_SET)) {
			ep->msg(ep, M_ERROR, "No previous regular expression.");
			return (1);
		}
		re = &ep->sre;
	} else {
		/* Set RE flags. */
		reflags = 0;
		if (ISSET(O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (ISSET(O_IGNORECASE))
			reflags |= REG_ICASE;

		/* Compile the RE. */
		re = &lre;
		if (eval = regcomp(re, (char *)ptrn, reflags)) {
			re_error(ep, eval, re);
			return (1);
		}

		/*
		 * Set saved RE.  Historic practice is that global set
		 * direction as well as the RE.
		 */
		ep->sre = lre;
		ep->searchdir = FORWARD;
		FF_SET(ep, F_RE_SET);
	}

	rval = 0;
	nchanged = 0;
	FF_SET(ep, F_IN_GLOBAL);

	/* For each line... */
	for (lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {

		/* Get the line. */
		if ((s = file_gline(ep, lno, &len)) == NULL) {
			GETLINE_ERR(ep, lno);
			rval = 1;
			goto err;
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
			re_error(ep, eval, re);
			rval = 1;
			goto err;
		}

		/*
		 * Execute the command, keeping track of the lines that
		 * change, and adjusting for inserted/deleted lines.
		 */
		ep->rptlines = 0;
		last1 = file_lline(ep);

		SCRLNO(ep) = lno;
		(void)snprintf((char *)cbuf, sizeof(cbuf), "%s", endp);
		if (ex_cmd(ep, cbuf)) {
			rval = 1;
			goto err;
		}

		last2 = file_lline(ep);
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
		SCRLNO(ep) = lno;

		/* Cumulative line change count. */
		nchanged += ep->rptlines;
		ep->rptlines = 0;
	}
	/* Cursor is on column 0, regardless. */
	SCRCOL(ep) = 0;

	/* Report statistics. */
err:	ep->rptlines += nchanged;
	ep->rptlabel = "changed";

	FF_CLR(ep, F_IN_GLOBAL);
	return (rval);
}
