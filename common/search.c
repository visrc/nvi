/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 5.36 1993/05/12 17:10:53 bostic Exp $ (Berkeley) $Date: 1993/05/12 17:10:53 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static int	check_delta __P((SCR *, EXF *, long, recno_t));
static int	check_word __P((SCR *, char **, int *, int *));
static int	ctag_conv __P((SCR *, char **, int *));
static int	get_delta __P((SCR *, char **, long *));
static int	resetup __P((SCR *, regex_t **, enum direction,
		    char *, char **, long *, int *, u_int));

/*
 * resetup --
 *	Set up a search for a regular expression.
 */
static int
resetup(sp, rep, dir, ptrn, epp, deltap, wordoffsetp, flags)
	SCR *sp;
	regex_t **rep;
	enum direction dir;
	char *ptrn, **epp;
	long *deltap;
	int *wordoffsetp;
	u_int flags;
{
	int delim, eval, re_flags, replaced;
	char *p, *t;

	/* Set return information the default. */
	*deltap = 0;
	*wordoffsetp = 0;

	/*
	 * Use saved pattern if no pattern supplied, or if only a delimiter
	 * character is supplied.  Only the pattern was saved, historic vi
	 * did not reuse any delta supplied.
	 */
	if (ptrn == NULL || ptrn[1] == '\0') {
		if (!F_ISSET(sp, S_RE_SET)) {
noprev:			msgq(sp, M_INFO, "No previous search pattern.");
			return (1);
		}
		*rep = &sp->sre;
		return (0);
	}

	re_flags = 0;				/* Set flags. */
	if (O_ISSET(sp, O_EXTENDED))
		re_flags |= REG_EXTENDED;
	if (O_ISSET(sp, O_IGNORECASE))
		re_flags |= REG_ICASE;

	if (LF_ISSET(SEARCH_PARSE)) {		/* Parse the string. */
		/* Set delimiter. */
		delim = *ptrn++;

		/* Find terminating delimiter, handling escaped delimiters. */
		for (p = t = ptrn;;)
			if (p[1] && p[0] == '\\') {
				++p;
				*t++ = *p++;
			} else if (p[0] == '\0' || p[0] == delim) {
				*t = '\0';
				++p;
				break;
			} else
				*t++ = *p++;

		/*
		 * If characters after the terminating delimiter, it may
		 * be an error, or may be an offset.  In either case, we
		 * return the end of the string, whatever it may be, or
		 * change the end pointer to reference a NULL.  Don't just
		 * whack the string, in case it's text space.
		 */
		if (*p) {
			if (LF_ISSET(SEARCH_TERM)) {
				msgq(sp, M_ERR,
				    "Characters after search string.");
				return (1);
			}
			if (get_delta(sp, &p, deltap))
				return (1);
			if (epp != NULL)
				*epp = p;
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
		if (ptrn == NULL || *ptrn == '\0') {
			if (!F_ISSET(sp, S_RE_SET))
				goto noprev;
			*rep = &sp->sre;
			return (0);
		}

		/* Replace any word search pattern. */
		if (check_word(sp, &ptrn, &replaced, wordoffsetp))
			return (1);
	} else if (LF_ISSET(SEARCH_TAG)) {
		if (ctag_conv(sp, &ptrn, &replaced))
			return (1);
		re_flags &= ~REG_EXTENDED;
	}

	/* Compile the RE. */
	if (eval = regcomp(*rep, (char *)ptrn, re_flags))
		re_error(sp, eval, *rep);
	else if (LF_ISSET(SEARCH_SET)) {
		F_SET(sp, S_RE_SET);
		sp->searchdir = dir;
		sp->sre = **rep;
	}

	/* Free up any extra memory. */
	if (replaced)
		if (ptrn == sp->gp->tmp_bp)
			F_CLR(sp->gp, G_TMP_INUSE);
		else
			free(ptrn);
	return (eval);
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
	recno_t lno;
	size_t coff, len;
	long delta;
	int eval, wordoffset, wrapped;
	char *l;

	if ((lno = file_lline(sp, ep)) == 0) {
		if (LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, EMPTYMSG);
		return (1);
	}

	re = &lre;
	if (resetup(sp, &re, FORWARD, ptrn, eptrn, &delta, &wordoffset, flags))
		return (1);

	/*
	 * Start searching immediately after the cursor.  If at the end of the
	 * line, start searching on the next line.  This is incompatible (read
	 * bug fix) with the historic vi -- searches for the '$' pattern never
	 * moved forward, and "-t foo" didn't work if "foo" was the first thing
	 * in the file.
	 */
	if (LF_ISSET(SEARCH_FILE)) {
		lno = 1;
		coff = 0;
	} else {
		if ((l = file_gline(sp, ep, fm->lno, &len)) == NULL) {
			GETLINE_ERR(sp, fm->lno);
			return (1);
		}
		if (fm->cno + 1 >= len) {
			if (fm->lno == lno) {
				if (!O_ISSET(sp, O_WRAPSCAN)) {
					if (LF_ISSET(SEARCH_MSG))
						msgq(sp, M_INFO, EOFMSG);
					return (1);
				}
				lno = 1;
			} else
				lno = fm->lno + 1;
			coff = 0;
		} else {
			lno = fm->lno;
			coff = fm->cno + 1;
		}
	}

	wrapped = 0;
	for (;; ++lno, coff = 0) {
		if ((l = file_gline(sp, ep, lno, &len)) == NULL) {
			if (wrapped) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, NOTFOUND);
				break;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, EOFMSG);
				break;
			}
			lno = 0;
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
		TRACE(sp, "F search: %lu from %u to %u\n",
		    lno, coff, len ? len - 1 : len);
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
		if (wrapped && O_ISSET(sp, O_WARN) && LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, WRAPMSG);

		/*
		 * If an offset, see if it's legal.  It's possible to match
		 * past the end of the line with $, so check for that case.
		 */
		if (delta) {
			if (check_delta(sp, ep, delta, lno))
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

			if (wordoffset)
				++rm->cno;

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
	recno_t lno;
	size_t coff, len, last;
	long delta;
	int eval, wordoffset, wrapped;
	char *l;

	if ((lno = file_lline(sp, ep)) == 0) {
		if (LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, EMPTYMSG);
		return (1);
	}

	re = &lre;
	if (resetup(sp, &re, BACKWARD, ptrn, eptrn, &delta, &wordoffset, flags))
		return (1);

	/* If in the first column, start searching on the previous line. */
	if (fm->cno == 0) {
		if (fm->lno == 1) {
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, SOFMSG);
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
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, SOFMSG);
				break;
			}
			if ((lno = file_lline(sp, ep)) == 0) {
				if (LF_ISSET(SEARCH_MSG))
					msgq(sp, M_INFO, EMPTYMSG);
				break;
			}
			++lno;
			wrapped = 1;
			continue;
		} else if (lno == fm->lno && wrapped) {
			if (LF_ISSET(SEARCH_MSG))
				msgq(sp, M_INFO, NOTFOUND);
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
		if (wrapped && O_ISSET(sp, O_WARN) && LF_ISSET(SEARCH_MSG))
			msgq(sp, M_INFO, WRAPMSG);
		
		if (delta) {
			if (check_delta(sp, ep, delta, lno))
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

			if (wordoffset)
				++rm->cno;
		}
		return (0);
	}
	return (1);
}

/*
 * check_word --
 *	Vi special cases the pattern "\<ptrn\>", doing "word" searches.
 */
static int
check_word(sp, ptrnp, replacedp, wordoffsetp)
	SCR *sp;
	char **ptrnp;
	int *replacedp, *wordoffsetp;
{
	GS *gp;
	size_t needspace;
	int cnt;
	char *p, *t, *mp;

	/* Count up the "word" patterns. */
	*replacedp = *wordoffsetp = 0;
	for (p = *ptrnp, cnt = 0; *p; ++p)
		if (p[0] == '\\' && p[1] && p[1] == '<')
			++cnt;
	if (cnt == 0)
		return (0);

	/* Report back if altered the start of the search pattern. */
	p = *ptrnp;
	if (p[0] == '\\' && p[1] == '<')
		*wordoffsetp = 1;

	/* Get enough memory to hold the final pattern. */
	needspace = strlen(*ptrnp) + cnt * sizeof(RE_NOTINWORD) * 2;
	gp = sp->gp;
	if (F_ISSET(gp, G_TMP_INUSE)) {
		if ((mp = malloc(needspace)) == NULL) {
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
			return (1);
		}
	} else {
		BINC(sp, gp->tmp_bp, gp->tmp_blen, needspace);
		mp = gp->tmp_bp;
		F_SET(gp, G_TMP_INUSE);
	}

	for (p = *ptrnp, t = mp; *p;)
		if (p[0] == '\\' && p[1] &&
		    p[1] == '<' || p[1] == '>') {
			memmove(t, RE_NOTINWORD, sizeof(RE_NOTINWORD) - 1);
			t += sizeof(RE_NOTINWORD) - 1;
			p += 2;
		} else
			*t++ = *p++;
	*t = '\0';

	*ptrnp = mp;
	*replacedp = 1;
	return (0);
}

/*
 * ctag_conv --
 *	Convert a tags search path into something that regex can handle.
 */
static int
ctag_conv(sp, ptrnp, replacedp)
	SCR *sp;
	char **ptrnp;
	int *replacedp;
{
	GS *gp;
	size_t blen, len;
	int lastdollar;
	char *bp, *p, *t;

	*replacedp = 0;

	len = strlen(p = *ptrnp);

	/* Max memory usage is 2 times the length of the string. */
	gp = sp->gp;
	if (F_ISSET(gp, G_TMP_INUSE)) {
		bp = NULL;
		blen = 0;
		BINC(sp, bp, blen, len * 2);
	} else {
		BINC(sp, gp->tmp_bp, gp->tmp_blen, len * 2);
		bp = gp->tmp_bp;
		F_SET(gp, G_TMP_INUSE);
	}

	t = bp;

	/* The last charcter is a '/' or '?', we just strip it. */
	if (p[len - 1] == '/' || p[len - 1] == '?')
		p[len - 1] = '\0';

	/* The next-to-last character is a '$', and it's magic. */
	if (p[len - 2] == '$') {
		lastdollar = 1;
		p[len - 2] = '\0';
	} else
		lastdollar = 0;

	/* The first character is a '/' or '?', we just strip it. */
	if (p[0] == '/' || p[0] == '?')
		++p;

	/* The second character is a '^', and it's magic. */
	if (p[0] == '^')
		*t++ = *p++;
		
	/*
	 * Escape every other magic character we can find, stripping the
	 * backslashes ctags inserts to escape the search delimiter
	 * characters.
	 */
	while (p[0]) {
		/* Ctags escapes the search delimiter characters. */
		if (p[0] == '\\' && (p[1] == '/' || p[1] == '?'))
			++p;
		else if (strchr("^.[$*", p[0]))
			*t++ = '\\';
		*t++ = *p++;
	}
	if (lastdollar)
		*t++ = '$';
	*t++ = '\0';

	*ptrnp = bp;
	*replacedp = 1;
	return (0);
}

/*
 * get_delta --
 *	Get a line delta.  The trickiness is that the delta can be pretty
 *	complicated, i.e. "+3-2+3" is allowed.
 */
static int
get_delta(sp, dp, valp)
	SCR *sp;
	char **dp;
	long *valp;
{
	long val, tval;

	for (tval = 0; **dp;) {
		if (!strchr("+-0123456789", **dp)) {
			msgq(sp, M_ERR, "Characters after delta string.");
			return (1);
		}
		errno = 0;
		val = strtol(*dp, dp, 10);
		if (errno == ERANGE) {
			if (val == LONG_MAX)
				msgq(sp, M_ERR, "Delta value overflow.");
			else if (val == LONG_MIN)
				msgq(sp, M_ERR, "Delta value underflow.");
			else
				msgq(sp, M_ERR, "Error: %s.", strerror(errno));
			return (1);
		}
		if (val >= 0) {
			if (LONG_MAX - val < tval) {
				msgq(sp, M_ERR, "Delta value overflow.");
				return (1);
			}
		} else
			if (-(LONG_MIN - tval) > val) {
				msgq(sp, M_ERR, "Delta value underflow.");
				return (1);
			}
		tval += val;
	}
	*valp = tval;
	return (0);
}

/*
 * check_delta --
 *	Check a line delta to see if it's legal.
 */
static int
check_delta(sp, ep, delta, lno)
	SCR *sp;
	EXF *ep;
	long delta;
	recno_t lno;
{
	if (delta < 0 && (recno_t)delta >= lno) {
		msgq(sp, M_ERR, "Search offset before line 1.");
		return (1);
	}
	if (file_gline(sp, ep, lno + delta, NULL) == NULL) {
		msgq(sp, M_ERR, "Search offset past end-of-file.");
		return (1);
	}
	return (0);
}

/*
 * re_error --
 *	Report a regular expression error.
 */
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
		msgq(sp, M_ERR, "Error: %s", strerror(errno));
	else {
		(void)regerror(errcode, preg, oe, s);
		msgq(sp, M_ERR, "RE error: %s", oe);
	}
	free(oe);
}
