/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 5.24 1993/04/06 11:36:31 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:36:31 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static int	checkdelta __P((SCR *, EXF *, recno_t, recno_t));
static int	resetup __P((SCR *, regex_t **, enum direction,
		    char *, char **, recno_t *, u_int));

static int
resetup(sp, rep, dir, ptrn, epp, deltap, flags)
	SCR *sp;
	regex_t **rep;
	enum direction dir;
	char *ptrn, **epp;
	recno_t *deltap;
	u_int flags;
{
	int eval, reflags;
	char *endp;
	char delim[2];

	if (ptrn == NULL && !F_ISSET(sp, S_RE_SET)) {
noprev:		msgq(sp, M_DISPLAY, "No previous search pattern.");
		return (1);
	}

	/* Set delta to default. */
	if (deltap != NULL)
		*deltap = 0;

	/*
	 * Use saved pattern if no pattern supplied, or if only the delimiter
	 * character is supplied.  Only the pattern is retained, historic vi
	 * did not reuse any delta supplied.
	 */
	if (ptrn == NULL || ptrn[1] == '\0') {
		*rep = &sp->sre;
		return (0);
	}

	reflags = 0;				/* Set flags. */
	if (O_ISSET(sp, O_EXTENDED))
		reflags |= REG_EXTENDED;
	if (O_ISSET(sp, O_IGNORECASE))
		reflags |= REG_ICASE;

	if (flags & SEARCH_PARSE) {		/* Parse the string. */
		/* Set delimiter. */
		delim[0] = *ptrn++;
		delim[1] = '\0';

		/* Find terminating delimiter. */
		endp = ptrn;
		ptrn = strsep(&endp, delim);

		/*
		 * If characters after the terminating delimiter, it may
		 * be an error, or may be an offset.  In either case, we
		 * return the end of the string, whatever it may be, or
		 * change the end pointer to reference a NULL.  Don't just
		 * whack the string, in case it's text space.
		 */
		if (endp != NULL && *endp != '\0') {
			if (flags & SEARCH_TERM) {
				msgq(sp, M_ERROR,
				    "Characters after search string.");
				return (1);
			}
			if (deltap != NULL)
				*deltap = strtol(endp, &endp, 10);
			if (epp != NULL)
				*epp = endp;
		} else {
			/*
			 * STATIC: NEVER WRITTEN.
			 * Can't be const, because the normal case isn't.
			 */
			static char ebuf[1];
			if (epp != NULL)
				*epp = ebuf;
		}

		/* If the pattern was empty, use the previous pattern. */
		if (*ptrn == '\0') {
			if (!F_ISSET(sp, S_RE_SET))
				goto noprev;
			*rep = &sp->sre;
		}
	}
						/* Compile the RE. */
	eval = regcomp(*rep, (char *)ptrn, reflags);
	if (eval != 0) {
		re_error(sp, eval, *rep);
		return (1);
	}
	if (flags & SEARCH_SET) {
		F_SET(sp, S_RE_SET);
		sp->searchdir = dir;
		sp->sre = **rep;
	}
	return (0);
}

#define	EMPTYMSG	"File empty; nothing to search."
#define	EOFMSG		"Reached end-of-file without finding the pattern."
#define	NOTFOUND	"Pattern not found."
#define	SOFMSG		"Reached top-of-file without finding the pattern."
#define	WRAPMSG		"Search wrapped."

int
f_search(sp, ep, fm, rm, ptrn, eptrn, flags)
	SCR *sp;
	EXF *ep;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int flags;
{
	regmatch_t match[1];
	regex_t *re, lre;
	recno_t delta, lno;
	size_t coff, len;
	int eval, wrapped;
	char *l;

	if ((lno = file_lline(sp, ep)) == 0) {
		if (flags & SEARCH_MSG)
			msgq(sp, M_DISPLAY, EMPTYMSG);
		return (1);
	}

	re = &lre;
	if (resetup(sp, &re, FORWARD, ptrn, eptrn, &delta, flags))
		return (1);

	/* If in the last column, start searching on the next line. */
	if ((l = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}
	if (fm->cno == len ? len - 1 : 0) {
		if (fm->lno == lno) {
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (flags & SEARCH_MSG)
					msgq(sp, M_DISPLAY, EOFMSG);
				return (1);
			}
			lno = 1;
		} else
			lno = fm->lno + 1;
	} else
		lno = fm->lno;

	wrapped = 0;
	for (coff = fm->cno ? fm->cno + 1 : 0;; ++lno, coff = 0) {
		if ((l = file_gline(sp, ep, lno, &len)) == NULL) {
			if (wrapped) {
				if (flags & SEARCH_MSG)
					msgq(sp, M_DISPLAY, NOTFOUND);
				break;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (flags & SEARCH_MSG)
					msgq(sp, M_DISPLAY, EOFMSG);
				break;
			}
			lno = 1;
			wrapped = 1;
			continue;
		}

		/* If already at EOL, just keep going. */
		if (len && coff == len)
			continue;

		/* Set the termination. */
		match[0].rm_so = coff;
		match[0].rm_eo = len;

#if defined(DEBUG) && defined(SEARCHDEBUG)
		TRACE(sp, "F search: %lu from %u to %u\n", lno, coff, len - 1);
#endif
		/* Search the line. */
		eval = regexec(re, (char *)l, 1, match,
		    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, re);
			break;
		}
		
		/* Warn if wrapped. */
		if (wrapped && O_ISSET(sp, O_WARN) && flags & SEARCH_MSG)
			msgq(sp, M_DISPLAY, WRAPMSG);

		/*
		 * If an offset, see if it's legal.  It's possible to match
		 * past the end of the line with $, so check for that case.
		 */
		if (delta) {
			if (checkdelta(sp, ep, delta, lno))
				break;
			rm->lno = delta + lno;
			rm->cno = 0;
		} else {
#if defined(DEBUG) && defined(SEARCHDEBUG)
			TRACE(sp, "found: %qu to %qu\n",
			    match[0].rm_so, match[0].rm_eo);
#endif
			rm->lno = lno;
			rm->cno = match[0].rm_so;
			if (rm->cno >= len)
				rm->cno = len ? len - 1 : 0;
		}
		return (0);
	}
	return (1);
}

int
b_search(sp, ep, fm, rm, ptrn, eptrn, flags)
	SCR *sp;
	EXF *ep;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int flags;
{
	regmatch_t match[1];
	regex_t *re, lre;
	size_t coff, len, last;
	recno_t delta, lno;
	int eval, wrapped;
	char *l;

	if ((lno = file_lline(sp, ep)) == 0) {
		if (flags & SEARCH_MSG)
			msgq(sp, M_DISPLAY, EMPTYMSG);
		return (1);
	}

	re = &lre;
	if (resetup(sp, &re, BACKWARD, ptrn, eptrn, &delta, flags))
		return (1);

	/* If in the first column, start searching on the previous line. */
	if (fm->cno == 0) {
		if (fm->lno == 1) {
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (flags & SEARCH_MSG)
					msgq(sp, M_DISPLAY, SOFMSG);
				return (1);
			}
		} else
			lno = fm->lno - 1;
	} else
		lno = fm->lno;

	wrapped = 0;
	for (coff = fm->cno;; --lno, coff = 0) {
		if (lno == 0) {
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (flags & SEARCH_MSG)
					msgq(sp, M_DISPLAY, SOFMSG);
				break;
			}
			if ((lno = file_lline(sp, ep)) == 0) {
				if (flags & SEARCH_MSG)
					msgq(sp, M_DISPLAY, EMPTYMSG);
				break;
			}
			wrapped = 1;
			continue;
		} else if (lno == fm->lno && wrapped) {
			if (flags & SEARCH_MSG)
				msgq(sp, M_DISPLAY, NOTFOUND);
			break;
		}

		if ((l = file_gline(sp, ep, lno, &len)) == NULL)
			return (1);

		/* Set the termination. */
		match[0].rm_so = 0;
		match[0].rm_eo = coff ? coff - 1 : len;

#if defined(DEBUG) && defined(SEARCHDEBUG)
		TRACE(sp, "B search: %lu from 0 to %qu\n", lno, match[0].rm_eo);
#endif
		/* Search the line. */
		eval = regexec(re, (char *)l, 1, match,
		    (match[0].rm_eo == len ? 0 : REG_NOTEOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, re);
			break;
		}

		/* Warn if wrapped. */
		if (wrapped && O_ISSET(sp, O_WARN) && flags & SEARCH_MSG)
			msgq(sp, M_DISPLAY, WRAPMSG);
		
		if (delta) {
			if (checkdelta(sp, ep, delta, lno))
				break;
			rm->lno = delta + lno;
			rm->cno = 0;
		} else {
#if defined(DEBUG) && defined(SEARCHDEBUG)
			TRACE(sp, "found: %qu to %qu\n",
			    match[0].rm_so, match[0].rm_eo);
#endif
			/*
			 * Find the last acceptable one in this line.  This
			 * is really painful, we need a cleaner interface to
			 * regexec to make this possible.
			 */
			for (;;) {
				last = match[0].rm_so;
				match[0].rm_so = match[0].rm_eo + 1;
				if (match[0].rm_so >= len)
					break;
				match[0].rm_eo = coff ? coff : len;
				eval = regexec(re,
				    (char *)l, 1, match, REG_STARTEND);
				if (eval == REG_NOMATCH)
					break;
				if (eval != 0) {
					re_error(sp, eval, re);
					return (1);
				}
			}
			rm->lno = lno;
			rm->cno = last;
		}
		return (0);
	}
	return (1);
}

static int
checkdelta(sp, ep, delta, lno)
	SCR *sp;
	EXF *ep;
	recno_t delta, lno;
{
	if (delta < 0 && (recno_t)delta >= lno) {
		msgq(sp, M_ERROR, "Search offset before line 1.");
		return (1);
	}
	if (file_gline(sp, ep, lno + delta, NULL) == NULL) {
		msgq(sp, M_ERROR, "Search offset past end-of-file.");
		return (1);
	}
	return (0);
}

void
re_error(sp, errcode, preg)
	SCR *sp;
	int errcode;
	regex_t *preg;
{
	size_t s;
	char *oe;

	s = regerror(errcode, preg, "", 0);
	if ((oe = malloc(s)) == NULL)
		msgq(sp, M_ERROR, "Error: %s", strerror(errno));
	else {
		(void)regerror(errcode, preg, oe, s);
		msgq(sp, M_ERROR, "RE error: %s", oe);
	}
	free(oe);
}
