/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 5.8 1992/11/04 09:17:09 bostic Exp $ (Berkeley) $Date: 1992/11/04 09:17:09 $";
#endif /* not lint */

#include <sys/types.h>

#include <db.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "search.h"
#include "extern.h"

enum direction searchdir = NOTSET;	/* Search direction. */

static regex_t sre;			/* Saved RE. */
static regmatch_t match[1];		/* Match table. */

static int	checkdelta __P((EXF *, recno_t, recno_t));
static int	resetup __P((regex_t **, enum direction,
		    u_char *, u_char **, recno_t *, u_int));

static int
resetup(rep, dir, ptrn, epp, deltap, flags)
	regex_t **rep;
	enum direction dir;
	u_char *ptrn, **epp;
	recno_t *deltap;
	u_int flags;
{
	int eval, reflags;
	u_char *ep;
	char delim[2];

	if (ptrn == NULL && searchdir == NOTSET) {
noprev:		msg("No previous search pattern.");
		return (1);
	}
	if (deltap != NULL)
		*deltap = 0;
	if (epp != NULL)
		*epp = NULL;
	if (ptrn) {
		reflags = 0;				/* Set flags. */
		if (ISSET(O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (ISSET(O_IGNORECASE))
			reflags |= REG_ICASE;
							/* Parse the string. */
		if (flags & SEARCH_PARSE) {
			delim[0] = *ptrn++;
			delim[1] = '\0';
			ep = ptrn;
			ptrn = USTRSEP(&ep, delim);
			if (deltap != NULL && ep != NULL)
				*deltap = USTRTOL(ep, &ep, 10);
			if (epp == NULL && *ep) {
				msg("Characters after search string.");
				return (1);
			}
			if (*ptrn == '\0') {
				if (searchdir == NOTSET)
					goto noprev;
				*rep = &sre;
			}
		}
							/* Compile RE. */
		eval = regcomp(*rep, (char *)ptrn, reflags);
		if (eval != 0) {
			re_error(eval, *rep);
			return (1);
		}
		if (flags & SEARCH_SET) {
			searchdir = dir;
			sre = **rep;
		}
	} else
		*rep = &sre;
	return (0);
}

#define	EMPTYMSG	"File empty; nothing to search."
#define	EOFMSG		"Reached end-of-file without finding the pattern."
#define	NOTFOUND	"Pattern not found."
#define	SOFMSG		"Reached top-of-file without finding the pattern."
#define	WRAPMSG		"Search wrapped."

MARK *
f_search(ep, fm, ptrn, eptrn, flags)
	EXF *ep;
	MARK *fm;
	u_char *ptrn, **eptrn;
	u_int flags;
{
	static MARK rval;
	regex_t *re, lre;
	recno_t delta, lno;
	size_t coff, len;
	int eval, wrapped;
	u_char *l;

	if ((lno = file_lline(ep)) == 0) {
		msg(EMPTYMSG);
		return (NULL);
	}

	re = &lre;
	if (resetup(&re, FORWARD, ptrn, eptrn, &delta, flags))
		return (NULL);

	/* If in the last column, start searching on the next line. */
	if ((l = file_gline(ep, fm->lno, &len)) == NULL) {
		GETLINE_ERR(fm->lno);
		return (NULL);
	}
	if (fm->cno == len ? len - 1 : 0) {
		if (fm->lno == lno) {
			if (!ISSET(O_WRAPSCAN)) {
				msg(EOFMSG);
				return (NULL);
			}
			lno = 1;
		} else
			lno = fm->lno + 1;
	} else
		lno = fm->lno;

	wrapped = 0;
	for (coff = fm->cno ? fm->cno + 1 : 0;; ++lno, coff = 0) {
		if ((l = file_gline(ep, lno, &len)) == NULL) {
			if (wrapped) {
				msg(NOTFOUND);
				break;
			}
			if (!ISSET(O_WRAPSCAN)) {
				msg(EOFMSG);
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
		TRACE("F search: %lu from %u to %u\n", lno, coff, len - 1);
#endif
		/* Search the line. */
		eval = regexec(re, (char *)l, 1, match,
		    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(eval, re);
			break;
		}
		
		/* Warn if wrapped. */
		if (wrapped && ISSET(O_WARN))
			msg(WRAPMSG);

		/*
		 * If an offset, see if it's legal.  It's possible to match
		 * past the end of the line with $, so check for that case.
		 */
		if (delta) {
			if (checkdelta(ep, delta, lno))
				break;
			rval.lno = delta + lno;
			rval.cno = 0;
		} else {
#if defined(DEBUG) && defined(SEARCHDEBUG)
			TRACE("found: %qu to %qu\n",
			    match[0].rm_so, match[0].rm_eo);
#endif
			rval.lno = lno;
			rval.cno = match[0].rm_so;
			if (rval.cno >= len)
				rval.cno = len ? len - 1 : 0;
		}
		return (&rval);
	}
	return (NULL);
}

MARK *
b_search(ep, fm, ptrn, eptrn, flags)
	EXF *ep;
	MARK *fm;
	u_char *ptrn, **eptrn;
	u_int flags;
{
	static MARK rval;
	regex_t *re, lre;
	size_t coff, len, last;
	recno_t delta, lno;
	int eval, wrapped;
	u_char *l;

	if ((lno = file_lline(ep)) == 0) {
		msg(EMPTYMSG);
		return (NULL);
	}

	re = &lre;
	if (resetup(&re, BACKWARD, ptrn, eptrn, &delta, flags))
		return (NULL);

	/* If in the first column, start searching on the previous line. */
	if (fm->cno == 0) {
		if (fm->lno == 1) {
			if (!ISSET(O_WRAPSCAN)) {
				msg(SOFMSG);
				return (NULL);
			}
		} else
			lno = fm->lno - 1;
	} else
		lno = fm->lno;

	wrapped = 0;
	for (coff = fm->cno;; --lno, coff = 0) {
		if (lno == 0) {
			if (!ISSET(O_WRAPSCAN)) {
				msg(SOFMSG);
				break;
			}
			if ((lno = file_lline(ep)) == 0) {
				msg(EMPTYMSG);
				break;
			}
			wrapped = 1;
			continue;
		} else if (lno == fm->lno && wrapped) {
			msg(NOTFOUND);
			break;
		}

		if ((l = file_gline(ep, lno, &len)) == NULL)
			return (NULL);

		/* Set the termination. */
		match[0].rm_so = 0;
		match[0].rm_eo = coff ? coff - 1 : len;

#if defined(DEBUG) && defined(SEARCHDEBUG)
		TRACE("B search: %lu from 0 to %qu\n", lno, match[0].rm_eo);
#endif
		/* Search the line. */
		eval = regexec(re, (char *)l, 1, match,
		    (match[0].rm_eo == len ? 0 : REG_NOTEOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(eval, re);
			break;
		}

		/* Warn if wrapped. */
		if (wrapped && ISSET(O_WARN))
			msg(WRAPMSG);
		
		if (delta) {
			if (checkdelta(ep, delta, lno))
				break;
			rval.lno = delta + lno;
			rval.cno = 0;
		} else {
#if defined(DEBUG) && defined(SEARCHDEBUG)
			TRACE("found: %qu to %qu\n",
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
					re_error(eval, re);
					return (NULL);
				}
			}
			rval.lno = lno;
			rval.cno = last;
		}
		return (&rval);
	}
	return (NULL);
}

static int
checkdelta(ep, delta, lno)
	EXF *ep;
	recno_t delta, lno;
{
	if (delta < 0 && (recno_t)delta >= lno) {
		msg("Search offset before line 1.");
		return (1);
	}
	if (file_gline(ep, lno + delta, NULL) == NULL) {
		msg("Search offset past end-of-file.");
		return (1);
	}
	return (0);
}

void
re_error(errcode, preg)
	int errcode;
	regex_t *preg;
{
	static char *oe;
	size_t s;

	if (oe != NULL)
		free(oe);
	s = regerror(errcode, preg, "", 0);
	if ((oe = malloc(s)) == NULL)
		msg("Error: %s", strerror(errno));
	else {
		(void)regerror(errcode, preg, oe, s);
		msg("RE error: %s", oe);
	}
}
