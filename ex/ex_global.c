/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_global.c,v 10.7 1995/09/21 10:57:40 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:57:40 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"

enum which {GLOBAL, V};

static int ex_g_setup __P((SCR *, EXCMD *, enum which));

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
	return (ex_g_setup(sp,
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
	return (ex_g_setup(sp, cmdp, V));
}

/*
 * ex_g_setup --
 *	Ex global and v commands.
 */
static int
ex_g_setup(sp, cmdp, cmd)
	SCR *sp;
	EXCMD *cmdp;
	enum which cmd;
{
	EXCMD *ecp;
	MARK abs;
	RANGE *rp;
	recno_t start, end;
	regex_t *re, lre;
	regmatch_t match[1];
	size_t len;
	int cnt, delim, eval, reflags, replaced;
	char *ptrn, *p, *t;

	NEEDFILE(sp, cmdp);

	if (F_ISSET(sp, S_EX_GLOBAL)) {
		msgq(sp, M_ERR,
	"124|The %s command can't be used as part of a global or v command",
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

	/* Get an EXCMD structure. */
	CALLOC_RET(sp, ecp, EXCMD *, 1, sizeof(EXCMD));
	CIRCLEQ_INIT(&ecp->rq);

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
	MALLOC_RET(sp, ecp->cp, char *, len * 2);
	ecp->o_cp = ecp->cp;
	memmove(ecp->cp + len, p, len);
	ecp->o_clen = len;
	ecp->range_lno = OOBLNO;
	FL_SET(ecp->agv_flags, cmd == GLOBAL ? AGV_GLOBAL : AGV_V);
	LIST_INSERT_HEAD(&sp->gp->ecq, ecp, q);

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
	for (start = cmdp->addr1.lno,
	    end = cmdp->addr2.lno; start <= end; ++start) {
		if (cnt-- == 0) {
			if (INTERRUPTED(sp)) {
				LIST_REMOVE(ecp, q);
				free(ecp->cp);
				free(ecp);
				break;
			}
			search_busy(sp, 1);
			cnt = INTERRUPT_CHECK;
		}
		if ((p = file_gline(sp, start, &len)) == NULL) {
			FILE_LERR(sp, start);
			return (1);
		}
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		switch (eval = regexec(&sp->sre, p, 0, match, REG_STARTEND)) {
		case 0:
			if (cmd == V)
				continue;
			break;
		case REG_NOMATCH:
			if (cmd == GLOBAL)
				continue;
			break;
		default:
			re_error(sp, eval, &sp->sre);
			break;
		}

		/* If follows the last entry, extend the last entry's range. */
		if ((rp = ecp->rq.cqh_last) != (void *)&ecp->rq &&
		    rp->stop == start - 1) {
			++rp->stop;
			continue;
		}

		/* Allocate a new range, and append it to the list. */
		CALLOC(sp, rp, RANGE *, 1, sizeof(RANGE));
		if (rp == NULL)
			return (1);
		rp->start = rp->stop = start;
		CIRCLEQ_INSERT_TAIL(&ecp->rq, rp, q);
	}
	search_busy(sp, 0);
	return (0);
}

/*
 * ex_g_insdel --
 *	Update the ranges based on an insertion or deletion.
 *
 * PUBLIC: void ex_g_insdel __P((SCR *, lnop_t, recno_t));
 */
void
ex_g_insdel(sp, op, lno)
	SCR *sp;
	lnop_t op;
	recno_t lno;
{
	EXCMD *ecp;
	RANGE *nrp, *rp;

	if (op == LINE_APPEND || op == LINE_RESET)
		return;

	for (ecp = sp->gp->ecq.lh_first; ecp != NULL; ecp = ecp->q.le_next) {
		if (!FL_ISSET(ecp->agv_flags, AGV_AT | AGV_GLOBAL | AGV_V))
			continue;
		for (rp = ecp->rq.cqh_first; rp != (void *)&ecp->rq; rp = nrp) {
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
					CIRCLEQ_REMOVE(&ecp->rq, rp, q);
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
				CIRCLEQ_INSERT_AFTER(&ecp->rq, rp, nrp, q);
				rp = nrp;
			}
		}

		/*
		 * If the command deleted/inserted lines, the cursor moves to
		 * the line after the deleted/inserted line.
		 */
		ecp->range_lno = lno;
	}
}

/*
 * ex_load --
 *	Load up the next command, which may be an @ buffer or global command.
 *
 * PUBLIC: int ex_load __P((SCR *));
 */
int
ex_load(sp)
	SCR *sp;
{
	GS *gp;
	EXCMD *ecp;
	RANGE *rp;

	F_CLR(sp, S_EX_GLOBAL);

	/*
	 * Lose any exhausted commands.  We know that the first command
	 * can't be an AGV command, which makes things a bit easier.
	 */
	for (gp = sp->gp;;) {
		if ((ecp = gp->ecq.lh_first) == &gp->excmd)
			return (0);
		if (FL_ISSET(ecp->agv_flags, AGV_ALL)) {
			/* Discard any exhausted ranges. */
			while ((rp = ecp->rq.cqh_first) != (void *)&ecp->rq)
				if (rp->start > rp->stop) {
					CIRCLEQ_REMOVE(&ecp->rq, rp, q);
					free(rp);
				} else
					break;

			/* If there's another range, continue with it. */
			if (rp != (void *)&ecp->rq)
				break;

			/* If it's a global/v command, fix up the last line. */
			if (FL_ISSET(ecp->agv_flags,
			    AGV_GLOBAL | AGV_V) && ecp->range_lno != OOBLNO)
				if (file_eline(sp, ecp->range_lno))
					sp->lno = ecp->range_lno;
				else {
					if (file_lline(sp, &sp->lno))
						return (1);
					if (sp->lno == 0)
						sp->lno = 1;
				}
			free(ecp->cp);
		} else
			if (ecp->clen != 0)
				return (0);

		/* Discard the EXCMD. */
		LIST_REMOVE(ecp, q);
		free(ecp);
	}

	/*
	 * We only get here if it's an active @, global or v command.  Set
	 * the current line number, and get a new copy of the command for
	 * the parser.  Note, the original pointer almost certainly moved,
	 * so we have play games.
	 */
	ecp->cp = ecp->o_cp;
	memmove(ecp->cp, ecp->cp + ecp->o_clen, ecp->o_clen);
	ecp->clen = ecp->o_clen;
	ecp->range_lno = sp->lno = rp->start++;

	if (FL_ISSET(ecp->agv_flags, AGV_GLOBAL | AGV_V))
		F_SET(sp, S_EX_GLOBAL);
	return (0);
}
