/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 10.2 1995/05/05 18:50:23 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:50:23 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

enum which {GLOBAL, V};

static int	global __P((SCR *, EXCMD *, enum which));
static int	gsrch __P((SCR *, EVENT *, int *));

/*
 * ex_global -- [line [,line]] g[lobal][!] /pattern/ [commands]
 *	Exec on lines matching a pattern.
 *
 * PUBLIC: int ex_global __P((SCR *, EXCMD *));
 */
int
ex_global(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (global(sp,
	    cmdp, FL_ISSET(cmdp->iflags, E_C_FORCE) ? V : GLOBAL));
}

/*
 * ex_v -- [line [,line]] v /pattern/ [commands]
 *	Exec on lines not matching a pattern.
 *
 * PUBLIC: int ex_v __P((SCR *, EXCMD *));
 */
int
ex_v(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	return (global(sp, cmdp, V));
}

/*
 * global --
 *	Ex global and v commands.
 */
static int
global(sp, cmdp, cmd)
	SCR *sp;
	EXCMD *cmdp;
	enum which cmd;
{
	regex_t *re, lre;
	GAT *gatp;
	MARK abs;
	size_t len;
	int delim, eval, reflags, replaced;
	char *ptrn, *p, *t;

	NEEDFILE(sp, cmdp->cmd);

#ifdef __TK__
Globals within globals can probably be permitted, now, as long as
we dont lose track of the S_GLOBAL bit, i.e. when to set/unset it.
#endif
	if (F_ISSET(sp, S_EX_GLOBAL)) {
		msgq(sp, M_ERR,
	"102|The %s command can't be used as part of a global command",
		    cmdp->cmd->name);
		return (1);
	}

	/*
	 * Skip leading white space.  Historic vi allowed any non-alphanumeric
	 * to serve as the global command delimiter.
	 */
	if (cmdp->argc == 0)
		goto usage;
	for (p = cmdp->argv[0]->bp; isblank(*p); ++p);
	if (*p == '\0' || isalnum(*p) ||
	    *p == '\\' || *p == '|' || *p == '\n') {
usage:		ex_message(sp, cmdp->cmd->usage, EXM_USAGE);
		return (1);
	}
	delim = *p++;

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
		if (p[0] == '\\')
			if (p[1] == delim)
				++p;
			else if (p[1] == '\\')
				*t++ = *p++;
		*t++ = *p++;
	}

	/* If the pattern string is empty, use the last one. */
	if (*ptrn == '\0') {
		if (!F_ISSET(sp, S_RE_SEARCH)) {
			ex_message(sp, NULL, EXM_NOPREVRE);
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
		F_SET(sp, S_RE_SEARCH);
	}

	/*
	 * The global commands sets the substitute RE as well as the
	 * everything-else RE.
	 */
	sp->subre = sp->sre;
	F_SET(sp, S_RE_SUBST);

	/* The global commands always set the previous context mark. */
	abs.lno = sp->lno;
	abs.cno = sp->cno;
	if (mark_set(sp, ABSMARK1, &abs, 1))
		return (1);

	/*
	 * Get a GAT structure.  We use the same structure for both global
	 * commands and @ buffer commands because they have to hang on the
	 * same stack, and they have lots of similarities.
	 */
	CALLOC_RET(sp, gatp, GAT *, 1, sizeof(GAT));
	CIRCLEQ_INIT(&gatp->rangeq);

	/*
	 * Save any remaining portion of the current command for restoration
	 * when this command is finished.
	 */
	if (cmdp->save_cmdlen != 0) {
		MALLOC_RET(sp, gatp->save_cmd, char *, cmdp->save_cmdlen);
		memmove(gatp->save_cmd, cmdp->save_cmd, cmdp->save_cmdlen);
		gatp->save_cmdlen = cmdp->save_cmdlen;
		cmdp->save_cmdlen = 0;
	}

	/*
	 * Get a copy of the command string; the default command is print.
	 * Don't worry about a set of <blank>s with no command, that will
	 * default to print in the ex parser.  We need to have two copies
	 * because the ex parser may step on the command string when it's
	 * parsing it.
	 */
	if ((len = strlen(p)) == 0) {
		p = "pp";
		len = 1;
	}
	MALLOC_RET(sp, gatp->cmd, char *, len * 2);
	memmove(gatp->cmd, p, len);
	gatp->cmd_len = len;
	gatp->range_lno = OOBLNO;
	F_SET(gatp, cmd == V ? GAT_ISV : GAT_ISGLOBAL);

	/* Set up search state. */
	gatp->gstart = cmdp->addr1.lno;
	gatp->gend = cmdp->addr2.lno;

	/* Add to the command queue. */
	LIST_INSERT_HEAD(&EXP(sp)->gatq, gatp, q);

	/* Set up the running function. */
	EXP(sp)->run_func = gsrch;
	sp->gp->cm_state = ES_RUNNING;
	sp->gp->cm_next = ES_PARSE_FUNC;
	FL_SET(sp->gp->ec_flags, EC_INTERRUPT);
	return (0);
}

/*
 * gsrch --
 *	Search the file for matches to the global pattern.
 */
static int
gsrch(sp, evp, completep)
	SCR *sp;
	EVENT *evp;
	int *completep;
{
	GAT *gatp;
	RANGE *rp;
	recno_t lno;
	regmatch_t match[1];
	size_t len;
	int cnt, eval;
	char *p;

	if (evp->e_event == E_INTERRUPT) {
		*completep = 1;
		return (0);
	}

	/*
	 * For each line...  The semantics of global matching are that we first
	 * have to decide which lines are going to get passed to the command,
	 * and then pass them to the command, ignoring other changes.  There's
	 * really no way to do this in a single pass, since arbitrary line
	 * creation, deletion and movement can be done in the ex command.  For
	 * example, a good vi clone test is ":g/X/mo.-3", or "g/X/.,.+1d".
	 * What we do is create linked list of lines that are tracked through
	 * each ex command.  There's a callback routine which the DB interface
	 * routines call when a line is created or deleted.  This doesn't help
	 * the layering much.
	 */
	cnt = INTERRUPT_CHECK;
	for (gatp = EXP(sp)->gatq.lh_first,
	    lno = gatp->gstart; lno <= gatp->gend; ++lno) {
		if (cnt-- == 0) {
			if (!F_ISSET(sp, S_BUSY))
				srch_busy(sp, 1);
			*completep = 0;
			return (0);
		}
		if ((p = file_gline(sp, lno, &len)) == NULL) {
			FILE_LERR(sp, lno);
			return (1);
		}
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		switch (eval = regexec(&sp->sre, p, 0, match, REG_STARTEND)) {
		case 0:
			if (F_ISSET(gatp, GAT_ISV))
				continue;
			break;
		case REG_NOMATCH:
			if (F_ISSET(gatp, GAT_ISGLOBAL))
				continue;
			break;
		default:
			re_error(sp, eval, &sp->sre);
			break;
		}

		/* If follows the last entry, extend the last entry's range. */
		if ((rp = gatp->rangeq.cqh_last) != (void *)&gatp->rangeq &&
		    rp->stop == lno - 1) {
			++rp->stop;
			continue;
		}

		/* Allocate a new range, and append it to the list. */
		CALLOC(sp, rp, RANGE *, 1, sizeof(RANGE));
		if (rp == NULL)
			return (1);
		rp->start = rp->stop = lno;
		CIRCLEQ_INSERT_TAIL(&gatp->rangeq, rp, q);
	}
	*completep = 1;
	return (0);
}

/*
 * global_insdel --
 *	Update the ranges based on an insertion or deletion.
 *
 * PUBLIC: void global_insdel __P((SCR *, lnop_t, recno_t));
 */
void
global_insdel(sp, op, lno)
	SCR *sp;
	lnop_t op;
	recno_t lno;
{
	GAT *gatp;
	RANGE *nrp, *rp;

	if (op == LINE_APPEND || op == LINE_RESET)
		return;

	for (gatp = EXP(sp)->gatq.lh_first;
	    gatp != NULL; gatp = gatp->q.le_next) {
		for (rp = gatp->rangeq.cqh_first;
		    rp != (void *)&gatp->rangeq; rp = nrp) {
			nrp = rp->q.cqe_next;

			/* If range less than the line, ignore it. */
			if (rp->stop < lno)
				continue;
			
			/*
			 * If range greater than the line, decrement or
			 * increment the range.
			 */
			if (rp->start > lno) {
				if (op == LINE_DELETE) {
					--rp->start;
					--rp->stop;
				} else {
					++rp->start;
					++rp->stop;
				}
				continue;
			}

			/*
			 * Lno is inside the range, decrement the end point
			 * for deletion, and split the range for insertion.
			 * In the latter case, since we're inserting a new
			 * element, neither range can be exhausted.
			 */
			if (op == LINE_DELETE) {
				if (rp->start > --rp->stop) {
					CIRCLEQ_REMOVE(&gatp->rangeq, rp, q);
					free(rp);
				}
			} else {
				/*
				 * XXX
				 * If this allocation fails, we're screwed.
				 */
				CALLOC(sp, nrp, RANGE *, 1, sizeof(RANGE));
				if (nrp == NULL)
					return;
				nrp->start = lno + 1;
				nrp->stop = rp->stop + 1;
				rp->stop = lno - 1;
				CIRCLEQ_INSERT_AFTER(&gatp->rangeq, rp, nrp, q);
				rp = nrp;
			}
		}

		/*
		 * If the command deleted/inserted lines, the cursor moves to
		 * the line after the deleted/inserted line.
		 */
		gatp->range_lno = lno;
	}
}
