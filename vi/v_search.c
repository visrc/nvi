/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_search.c,v 9.2 1994/11/17 18:53:08 bostic Exp $ (Berkeley) $Date: 1994/11/17 18:53:08 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
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
#include "vcmd.h"
#include "excmd.h"

static int addr_search __P((SCR *, VICMDARG *, enum direction));
static int correct __P((SCR *, VICMDARG *, int));
static int getptrn __P((SCR *, ARG_CHAR_T, char **, size_t *));
static int search __P((SCR *,
    VICMDARG *, char *, size_t, u_int, enum direction));

/*
 * v_searchb -- [count]?RE[? offset]
 *	Search backward.
 */
int
v_searchb(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	return (addr_search(sp, vp, BACKWARD));
}

/*
 * v_searchf -- [count]/RE[/ offset]
 *	Search forward.
 */
int
v_searchf(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	return (addr_search(sp, vp, FORWARD));
}

static int
addr_search(sp, vp, dir)
	SCR *sp;
	VICMDARG *vp;
	enum direction dir;
{
	MARK save;
	size_t len;
	char *ptrn;
	int isdelta, nb, notused;

	/*
	 * !!!
	 * If using the search command as a motion, any addressing components
	 * are lost, i.e. y/ptrn/+2, when repeated, is the same as y/ptrn/.
	 */
	if (F_ISSET(vp, VC_ISDOT))
		return (search(sp, vp,
		    NULL, len, SEARCH_MSG | SEARCH_SET, dir));

	/* Get a pattern.  A zero-length pattern terminates the command. */
	if (getptrn(sp, dir == BACKWARD ? CH_BSEARCH : CH_FSEARCH, &ptrn, &len))
		return (1);
	if (len == 0) {
		F_SET(vp, VM_NOMOTION);
		return (0);
	}

	/*
	 * !!!
	 * Historically, vi / and ? commands were full-blown ex addresses,
	 * including ';' delimiters, trailing <blank>'s, multiple search
	 * strings (separated by semi-colons) and, finally, full-blown z
	 * commands after the / and ? search strings.  (If the search was
	 * being used as a motion, the trailing z command was ignored.)
	 * For multiple search strings, leading <blank>'s at the second and
	 * subsequent strings were eaten as well.  This has some unintended
	 * side-effects: the command /ptrn/;3 is legal and results in moving
	 * to line 3.  Should have been illegal, but it's too late now.
	 *
	 * !!!
	 * Historically, if any part of the search command failed, the cursor
	 * remained unmodified (even if ; was used).  We have to play games
	 * because the underlying ex parser thinks we're modifying the cursor
	 * as we go, but I think we're compatible with historic practice.
	 *
	 * !!!
	 * Historically, the command "/STRING/;   " failed, apparently it
	 * confused the parser.  We're not that compatible.
	 *
	 * !!!
	 * Historically, if it wasn't a motion command, a delta in the search
	 * pattern turns it into a first nonblank movement.
	 */
	save.lno = sp->lno;
	save.cno = sp->cno;
	for (nb = 0;;) {
		for (; len > 0 && isblank(*ptrn); ++ptrn, --len);
		if (len == 0)
			break;

		if (ex_line(sp, &vp->m_stop, &ptrn, &len, &notused, &isdelta))
			goto err;
		if (isdelta)
			nb = 1;
		sp->lno = vp->m_stop.lno;
		sp->cno = vp->m_stop.cno;

		for (; len > 0 && isblank(*ptrn); ++ptrn, --len);
		if (len == 0)
			break;
		switch (*ptrn) {
		case ';':	/* Ignore; the cursor is already udpated. */
			++ptrn;
			--len;
			break;
		case 'z':	/* Push the z command on the stack. */
			if (!ISMOTION(vp) &&
			    term_push(sp, ptrn, len, CH_NOMAP | CH_QUOTED))
				return (1);
			len = 0;
			break;
		default:
			msgq(sp, M_ERR,
		    "188|Characters after search string and/or line offset");
err:			sp->lno = save.lno;
			sp->cno = save.cno;
			return (1);
		}
	}
	sp->lno = save.lno;
	sp->cno = save.cno;

	/* Non-motion commands move to the end of the range. */
	if (ISMOTION(vp)) {
		if (correct(sp, vp, isdelta))
			return (1);
	} else {
		vp->m_final = vp->m_stop;
		if (nb) {
			F_CLR(vp, VM_RCM_MASK);
			F_SET(vp, VM_RCM_SETFNB);
		}
	}
	return (0);
}

/*
 * v_searchN -- N
 *	Reverse last search.
 */
int
v_searchN(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	enum direction dir;

	switch (sp->searchdir) {
	case BACKWARD:
		dir = FORWARD;
		break;
	case FORWARD:
		dir = BACKWARD;
		break;
	default:
		dir = sp->searchdir;
		break;
	}
	return (search(sp, vp, NULL, 0, SEARCH_MSG, dir));
}

/*
 * v_searchn -- n
 *	Repeat last search.
 */
int
v_searchn(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	return (search(sp, vp, NULL, 0, SEARCH_MSG, sp->searchdir));
}

/*
 * v_searchw -- [count]^A
 *	Search for the word under the cursor.
 */
int
v_searchw(sp, vp)
	SCR *sp;
	VICMDARG *vp;
{
	size_t blen, len;
	int rval;
	char *bp;

	len = vp->kbuflen + sizeof(RE_WSTART) + sizeof(RE_WSTOP);
	GET_SPACE_RET(sp, bp, blen, len);
	(void)snprintf(bp, blen, "%s%s%s", RE_WSTART, vp->keyword, RE_WSTOP);

	rval = search(sp,
	    vp, bp, 0, SEARCH_MSG | SEARCH_SET | SEARCH_TERM, FORWARD);

	FREE_SPACE(sp, bp, blen);
	return (rval);
}

static int
search(sp, vp, ptrn, len, flags, dir)
	SCR *sp;
	VICMDARG *vp;
	u_int flags;
	char *ptrn;
	size_t len;
	enum direction dir;
{
	char *notused;

	if (ISMOTION(vp))
		flags |= SEARCH_EOL;

	switch (dir) {
	case BACKWARD:
		if (b_search(sp,
		    &vp->m_start, &vp->m_stop, ptrn, &notused, &flags))
			return (1);
		break;
	case FORWARD:
		if (f_search(sp,
		    &vp->m_start, &vp->m_stop, ptrn, &notused, &flags))
			return (1);
		break;
	case NOTSET:
		msgq(sp, M_ERR, "187|No previous search pattern");
		return (1);
	default:
		abort();
		/* NOTREACHED */
	}

	/* Non-motion commands move to the end of the range. */
	if (ISMOTION(vp)) {
		if (correct(sp, vp, 0))
			return (1);
	} else
		vp->m_final = vp->m_stop;
	return (0);
}

/*
 * getptrn --
 *	Get the search pattern.
 */
static int
getptrn(sp, prompt, ptrnp, lenp)
	SCR *sp;
	ARG_CHAR_T prompt;
	char **ptrnp;
	size_t *lenp;
{
	TEXT *tp;

	if (sp->s_get(sp, sp->tiqp, prompt,
	    TXT_BS | TXT_CR | TXT_ESCAPE | TXT_PROMPT) != INP_OK)
		return (1);

	/* Len is 0 if backspaced over the prompt, 1 if only CR entered. */
	tp = sp->tiqp->cqh_first;
	*ptrnp = tp->lb;
	*lenp = tp->len;
	return (0);
}

/*
 * correct --
 *	Handle command with a search as the motion.
 *
 * !!!
 * Historically, commands didn't affect the line searched to/from if the
 * motion command was a search and the final position was the start/end
 * of the line.  There were some special cases and vi was not consistent;
 * it was fairly easy to confuse it.  For example, given the two lines:
 *
 *	abcdefghi
 *	ABCDEFGHI
 *
 * placing the cursor on the 'A' and doing y?$ would so confuse it that 'h'
 * 'k' and put would no longer work correctly.  In any case, we try to do
 * the right thing, but it's not going to exactly match historic practice.
 */
static int
correct(sp, vp, isdelta)
	SCR *sp;
	VICMDARG *vp;
	int isdelta;
{
	enum direction dir;
	MARK m;
	size_t len;

	/*
	 * !!!
	 * We may have wrapped if wrapscan was set, and we may have returned
	 * to the position where the cursor started.  Historic vi didn't cope
	 * with this well.  Yank wouldn't beep, but the first put after the
	 * yank would move the cursor right one column (without adding any
	 * text) and the second would put a copy of the current line.  The
	 * change and delete commands would beep, but would leave the cursor
	 * on the colon command line.  I believe that there are macros that
	 * depend on delete, at least, failing.  For now, commands that use
	 * search as a motion component fail when the search returns to the
	 * original cursor position.
	 */
	if (vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno == vp->m_stop.cno) {
		msgq(sp, M_BERR, "189|Search wrapped to original position");
		return (1);
	}

	/*
	 * !!!
	 * Searches become line mode operations if there was a delta specified
	 * to the search pattern.
	 */
	if (isdelta)
		F_SET(vp, VM_LMODE);

	/*
	 * If the motion is in the reverse direction, switch the start and
	 * stop MARK's so that it's in a forward direction.  (There's no
	 * reason for this other than to make the tests below easier.  The
	 * code in vi.c:vi() would have done the switch.)  Both forward
	 * and backward motions can happen for any kind of search command
	 * because of the wrapscan option.
	 */
	if (vp->m_start.lno > vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno > vp->m_stop.cno) {
		m = vp->m_start;
		vp->m_start = vp->m_stop;
		vp->m_stop = m;
		dir = BACKWARD;
	} else
		dir = FORWARD;

	/*
	 * BACKWARD:
	 *	Delete and yank commands move to the end of the range.
	 *	Ignore others.
	 *
	 * FORWARD:
	 *	Delete and yank commands don't move.  Ignore others.
	 */
	vp->m_final = vp->m_start;

	/*
	 * !!!
	 * Delta'd searches don't correct based on column positions.
	 */
	if (isdelta)
		return (0);

	/*
	 * !!!
	 * Backward searches starting at column 0, and forward searches ending
	 * at column 0 are corrected to the last column of the previous line.
	 * Otherwise, adjust the starting/ending point to the character before
	 * the current one (this is safe because we know the search had to move
	 * to succeed).
	 *
	 * Searches become line mode operations if they start at column 0 and
	 * end at column 0 of another line.
	 */
	if (vp->m_start.lno < vp->m_stop.lno && vp->m_stop.cno == 0) {
		if (file_gline(sp, --vp->m_stop.lno, &len) == NULL) {
			GETLINE_ERR(sp, vp->m_stop.lno);
			return (1);
		}
		if (vp->m_start.cno == 0)
			F_SET(vp, VM_LMODE);
		vp->m_stop.cno = len ? len - 1 : 0;
	} else
		--vp->m_stop.cno;

	return (0);
}
