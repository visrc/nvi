/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 8.23 1993/12/02 10:49:42 bostic Exp $ (Berkeley) $Date: 1993/12/02 10:49:42 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
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
	return (global(sp, ep,
	    cmdp, F_ISSET(cmdp, E_FORCE) ? VGLOBAL : GLOBAL));
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
	struct sigaction act, oact;
	struct termios nterm, term;
	recno_t elno, last1, last2, lno;
	regmatch_t match[1];
	regex_t *re, lre;
	size_t clen, len;
	u_int istate;
	int delim, eval, isig, reflags, replaced, rval;
	char *cb, *ptrn, *p, *t;

	/*
	 * Skip leading white space.  Historic vi allowed any non-
	 * alphanumeric to serve as the global command delimiter.
	 */
	for (p = cmdp->argv[0]->bp; isblank(*p); ++p);
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
			/*
			 * !!!
			 * Nul terminate the pattern string -- it's passed
			 * to regcomp which doesn't understand anything else.
			 */
			*t = '\0';
			break;
		}
		if (p[0] == '\\' && p[1] == delim)
			++p;
		*t++ = *p++;
	}

	/* Get the command string. */
	if ((clen = strlen(p)) == 0) {
		msgq(sp, M_ERR, "No command string specified.");
		return (1);
	}
	if ((cb = malloc(clen)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	memmove(cb, p, clen);

	/* If the pattern string is empty, use the last one. */
	if (*ptrn == '\0') {
		if (!F_ISSET(sp, S_SRE_SET)) {
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

		/* Convert vi-style RE's to POSIX 1003.2 RE's. */
		if (re_conv(sp, &ptrn, &replaced))
			return (1);

		/* Compile the RE. */
		re = &lre;
		eval = regcomp(re, ptrn, reflags);

		/* Free up any allocated memory. */
		if (replaced)
			FREE_SPACE(sp, ptrn, 0);

		if (eval) {
			re_error(sp, eval, re);
			return (1);
		}

		/*
		 * Set saved RE.  Historic practice is that
		 * globals set direction as well as the RE.
		 */
		sp->sre = lre;
		sp->searchdir = FORWARD;
		F_SET(sp, S_SRE_SET);
	}

	/*
	 * The global commands sets the substitute RE as well as
	 * the everything-else RE.
	 */
	sp->subre = sp->sre;
	F_SET(sp, S_SUBRE_SET);

	F_SET(sp, S_GLOBAL);

	/*
	 * Command interrupts.
	 *
	 * ISIG turns on VINTR, VQUIT and VSUSP.  We want VINTR to interrupt
	 * the command, so we install a handler.  VQUIT is ignored by main()
	 * because nvi never wants to catch it.  A handler for VSUSP should
	 * have been installed by the screen code.
	 */
	if (F_ISSET(sp->gp, G_ISFROMTTY)) {
		act.sa_handler = global_intr;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		if (isig = !sigaction(SIGINT, &act, &oact)) {
			istate = F_ISSET(sp, S_INTERRUPTIBLE);
			F_CLR(sp, S_INTERRUPTED);
			F_SET(sp, S_INTERRUPTIBLE);
			if (tcgetattr(STDIN_FILENO, &term)) {
				msgq(sp, M_SYSERR, "tcgetattr");
				return (1);
			}
			nterm = term;
			nterm.c_lflag |= ISIG;
			if (tcsetattr(STDIN_FILENO,
			    TCSANOW | TCSASOFT, &nterm)) {
				msgq(sp, M_SYSERR, "tcsetattr");
				return (1);
			}
		}
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
		if (ex_cmd(sp, ep, cb, clen))
			goto err;

		/* Someone's unhappy, time to stop. */
		if (F_ISSET(sp, S_INTERRUPTED)) {
			msgq(sp, M_INFO, "Interrupted.");
			break;
		}

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
	F_CLR(sp, S_GLOBAL);

	if (F_ISSET(sp->gp, G_ISFROMTTY) && isig) {
		if (sigaction(SIGINT, &oact, NULL))
			msgq(sp, M_SYSERR, "signal");
		if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &term))
			msgq(sp, M_SYSERR, "tcsetattr");
		F_CLR(sp, S_INTERRUPTED);
		if (!istate)
			F_CLR(sp, S_INTERRUPTIBLE);
	}
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

	for (sp = __global_list->dq.cqh_first;
	    sp != (void *)&__global_list->dq; sp = sp->q.cqe_next)
		if (F_ISSET(sp, S_GLOBAL) && F_ISSET(sp, S_INTERRUPTIBLE))
			F_SET(sp, S_INTERRUPTED);
}
