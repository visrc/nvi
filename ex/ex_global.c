/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 8.9 1993/09/10 10:26:16 bostic Exp $ (Berkeley) $Date: 1993/09/10 10:26:16 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"

enum which {GLOBAL, VGLOBAL};

static int	global __P((SCR *, EXF *, EXCMDARG *, enum which));
static void	global_intr __P((int));

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
	struct termios nterm, term;
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
	for (p = cmdp->argv[0]; isblank(*p); ++p);
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

	/* Turn on interrupts and install an interrupt catcher. */
	F_SET(sp, S_GLOBAL | S_INTERRUPTIBLE);
	(void)signal(SIGINT, global_intr);
	if (tcgetattr(STDIN_FILENO, &term)) {
		msgq(sp, M_ERR, "global: tcgetattr: %s", strerror(errno));
		return (1);
	}
	nterm = term;
	nterm.c_lflag |= ISIG;
	if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &nterm)) {
		msgq(sp, M_ERR, "global: tcsetattr: %s", strerror(errno));
		return (1);
	}

	/* For each line... */
	for (rval = 0, lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {

		/* Get the line. */
		if ((t = file_gline(sp, ep, lno, &len)) == NULL) {
			GETLINE_ERR(sp, lno);
			goto err;
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
			goto err;
		}

		/*
		 * Execute the command, keeping track of the lines that
		 * change, and adjusting for inserted/deleted lines.
		 */
		if (file_lline(sp, ep, &last1))
			goto err;

		sp->lno = lno;
		(void)snprintf(cbuf, sizeof(cbuf), "%s", p);
		if (ex_cmd(sp, ep, cbuf, 0))
			goto err;

		/* Someone's unhappy, time to stop. */
		if (F_ISSET(sp, S_INTERRUPTED))
			break;

		if (file_lline(sp, ep, &last2)) {
err:			rval = 1;
			break;
		}
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

	/* Restore ex/vi terminal settings. */
	if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &term))
		msgq(sp, M_ERR, "tcsetattr: %s", strerror(errno));

	F_CLR(sp, S_GLOBAL);
	return (rval);
}

/*
 * global_intr --
 *	Set the interrupt bit in any screen that is running an interruptible
 *	global.
 *
 * XXX
 * In the future this may be a problem.  The user should be able to move to
 * another screen and keep typing while this runs.  If so, and the user has
 * more than one global running, it will be hard to decide which one to
 * stop.
 */
static void
global_intr(signo)
	int signo;
{
	SCR *sp;

	for (sp = __global_list->scrhdr.next;
	     sp != (SCR *)&__global_list->scrhdr; sp = sp->next)
		if (F_ISSET(sp, S_GLOBAL) && F_ISSET(sp, S_INTERRUPTIBLE))
			F_SET(sp, S_INTERRUPTED);
}
