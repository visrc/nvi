/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 10.7 1995/09/21 12:06:18 bostic Exp $ (Berkeley) $Date: 1995/09/21 12:06:18 $";
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

#include "common.h"

typedef enum { S_EMPTY, S_EOF, S_NOPREV, S_NOTFOUND, S_SOF, S_WRAP } smsg_t;

static int	ctag_conv __P((SCR *, char **, int *));
static void	search_msg __P((SCR *, smsg_t));

/*
 * search_setup --
 *	Set up a search.
 *
 * PUBLIC: int search_setup __P((SCR *, dir_t, char *, char **, u_int));
 */
int
search_setup(sp, dir, ptrn, epp, flags)
	SCR *sp;
	dir_t dir;
	char *ptrn, **epp;
	u_int flags;
{
	recno_t lno;
	regex_t re;
	int delim, eval, re_flags, replaced;
	char *p, *t;

	/* If the file is empty, it's a fast search. */
	if (sp->lno <= 1) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno == 0) {
			if (LF_ISSET(SEARCH_MSG))
				search_msg(sp, S_EMPTY);
			return (1);
		}
	}

	replaced = 0;
	if (LF_ISSET(SEARCH_PARSE)) {		/* Parse the string. */
		/*
		 * Use saved pattern if no pattern supplied, or if only the
		 * delimiter character is supplied.  Only the pattern was
		 * saved, historiclly vi didn't reuse addressing or delta
		 * information.
		 */
		if (ptrn == NULL)
			goto prev;
		if (ptrn[1] == '\0') {
			if (epp != NULL)
				*epp = ptrn + 1;
			goto prev;
		}
		if (ptrn[0] == ptrn[1]) {
			if (epp != NULL)
				*epp = ptrn + 2;
prev:			if (!F_ISSET(sp, S_RE_SEARCH)) {
				search_msg(sp, S_NOPREV);
				return (1);
			}
			/*
			 * May need to recompile if an edit option was changed.
			 * The options code doesn't check error cases, it just
			 * sets a bit -- be careful.   If we can't recompile
			 * for some reason, quit, someone else already reported
			 * the out-of-memory message.
			 */
			if (F_ISSET(sp, S_RE_RECOMPILE)) {
				if (sp->re == NULL) {
					search_msg(sp, S_NOPREV);
					return (1);
				}
				ptrn = sp->re;
				goto recomp;
			}

			if (LF_ISSET(SEARCH_SET))
				sp->searchdir = dir;
			return (0);
		}

		/*
		 * Set delimiter, and move forward to terminating delimiter,
		 * handling escaped delimiters.
		 *
		 * QUOTING NOTE:
		 * Only toss an escape character if it escapes a delimiter.
		 */
		for (delim = *ptrn, p = t = ++ptrn;; *t++ = *p++) {
			if (p[0] == '\0' || p[0] == delim) {
				if (p[0] == delim)
					++p;
				*t = '\0';
				break;
			}
			if (p[1] == delim && p[0] == '\\')
				++p;
		}
		if (epp != NULL)
			*epp = p;
		if (re_conv(sp, &ptrn, &replaced))
			return (1);
	} else if (LF_ISSET(SEARCH_TAG) && ctag_conv(sp, &ptrn, &replaced))
		return (1);

	/*
	 * It's not enough to save only the compiled RE; changing the
	 * edit options that modify search (e.g. extended, ignorecase)
	 * require that we recompile the RE.
	 */
	if (sp->re != NULL)
		free(sp->re);
	sp->re = v_strdup(sp, ptrn, sp->re_len = strlen(ptrn));

	/* Set up the flags and compile the RE. */
recomp:	re_flags = 0;
	if (!LF_ISSET(SEARCH_TAG)) {
		if (O_ISSET(sp, O_EXTENDED))
			re_flags |= REG_EXTENDED;
		if (O_ISSET(sp, O_IGNORECASE))
			re_flags |= REG_ICASE;
	}
	if (eval = regcomp(&re, ptrn, re_flags))
		re_error(sp, eval, &re);
	else {
		if (LF_ISSET(SEARCH_SET))
			sp->searchdir = dir;
		if (LF_ISSET(SEARCH_SET) || F_ISSET(sp, S_RE_RECOMPILE)) {
			sp->sre = re;
			F_SET(sp, S_RE_SEARCH);
		}
		F_CLR(sp, S_RE_RECOMPILE);
	}

	/* Free up any extra memory. */
	if (replaced)
		FREE_SPACE(sp, ptrn, 0);
	return (eval);
}

/*
 * f_search --
 *	Do a forward search.
 *
 * PUBLIC: int f_search __P((SCR *, MARK *, MARK *, char *, char **, u_int));
 */
int
f_search(sp, fm, rm, ptrn, eptrn, flags)
	SCR *sp;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int flags;
{
	recno_t lno;
	regmatch_t match[1];
	size_t coff, len;
	int cnt, eval, rval, wrapped;
	char *l;

	if (search_setup(sp, FORWARD, ptrn, eptrn, flags))
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
		if ((l = file_gline(sp, fm->lno, &len)) == NULL) {
			FILE_LERR(sp, fm->lno);
			return (1);
		}
		if (fm->cno + 1 >= len) {
			lno = fm->lno + 1;
			if ((l = file_gline(sp, lno, &len)) == NULL) {
				if (!O_ISSET(sp, O_WRAPSCAN)) {
					if (LF_ISSET(SEARCH_MSG))
						search_msg(sp, S_EOF);
					return (1);
				}
				lno = 1;
			}
			coff = 0;
		} else {
			lno = fm->lno;
			coff = fm->cno + 1;
		}
	}

	for (cnt = INTERRUPT_CHECK, rval = 1, wrapped = 0;; ++lno, coff = 0) {
		if (cnt-- == 0) {
			if (INTERRUPTED(sp))
				break;
			search_busy(sp, 1);
			cnt = INTERRUPT_CHECK;
		}
		if (wrapped && lno > fm->lno ||
		    (l = file_gline(sp, lno, &len)) == NULL) {
			if (wrapped) {
				if (LF_ISSET(SEARCH_MSG))
					search_msg(sp, S_NOTFOUND);
				break;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					search_msg(sp, S_EOF);
				break;
			}
			lno = 0;
			wrapped = 1;
			continue;
		}

		/* If already at EOL, just keep going. */
		if (len != 0 && coff == len)
			continue;

		/* Set the termination. */
		match[0].rm_so = coff;
		match[0].rm_eo = len;

#if defined(DEBUG) && 0
		TRACE(sp, "F search: %lu from %u to %u\n",
		    lno, coff, len != 0 ? len - 1 : len);
#endif
		/* Search the line. */
		eval = regexec(&sp->sre, l, 1, match,
		    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, &sp->sre);
			break;
		}

		/* Warn if wrapped. */
		if (wrapped && O_ISSET(sp, O_WARN) && LF_ISSET(SEARCH_MSG))
			search_msg(sp, S_WRAP);

#if defined(DEBUG) && 0
		TRACE(sp, "F search: %qu to %qu\n",
		    match[0].rm_so, match[0].rm_eo);
#endif
		rm->lno = lno;
		rm->cno = match[0].rm_so;

		/*
		 * If a change command, it's possible to move beyond the end
		 * of a line.  Historic vi generally got this wrong (e.g. try
		 * "c?$<cr>").  Not all that sure this gets it right, there
		 * are lots of strange cases.
		 */
		if (!LF_ISSET(SEARCH_EOL) && rm->cno >= len)
			rm->cno = len != 0 ? len - 1 : 0;

		rval = 0;
		break;
	}

	search_busy(sp, 0);
	return (rval);
}

/*
 * b_search --
 *	Do a backward search.
 *
 * PUBLIC: int b_search __P((SCR *, MARK *, MARK *, char *, char **, u_int));
 */
int
b_search(sp, fm, rm, ptrn, eptrn, flags)
	SCR *sp;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int flags;
{
	recno_t lno;
	regmatch_t match[1];
	size_t coff, last, len;
	int cnt, eval, rval, wrapped;
	char *l;

	if (search_setup(sp, BACKWARD, ptrn, eptrn, flags))
		return (1);

	/* If in the first column, start search on the previous line. */
	if (fm->cno == 0) {
		if (fm->lno == 1) {
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					search_msg(sp, S_SOF);
				return (1);
			}
		} else
			lno = fm->lno - 1;
	} else
		lno = fm->lno;
	coff = fm->cno;

	for (cnt = INTERRUPT_CHECK, rval = 1, wrapped = 0;; --lno, coff = 0) {
		if (cnt-- == 0) {
			if (INTERRUPTED(sp))
				break;
			search_busy(sp, 1);
			cnt = INTERRUPT_CHECK;
		}
		if (wrapped && lno < fm->lno || lno == 0) {
			if (wrapped) {
				if (LF_ISSET(SEARCH_MSG))
					search_msg(sp, S_NOTFOUND);
				break;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (LF_ISSET(SEARCH_MSG))
					search_msg(sp, S_SOF);
				break;
			}
			if (file_lline(sp, &lno))
				break;
			if (lno == 0) {
				if (LF_ISSET(SEARCH_MSG))
					search_msg(sp, S_EMPTY);
				break;
			}
			++lno;
			wrapped = 1;
			continue;
		}

		if ((l = file_gline(sp, lno, &len)) == NULL)
			break;

		/* Set the termination. */
		match[0].rm_so = 0;
		match[0].rm_eo = len;

#if defined(DEBUG) && 0
		TRACE(sp, "B search: %lu from 0 to %qu\n", lno, match[0].rm_eo);
#endif
		/* Search the line. */
		eval = regexec(&sp->sre, l, 1, match,
		    (match[0].rm_eo == len ? 0 : REG_NOTEOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, &sp->sre);
			break;
		}

		/* Check for a match starting past the cursor. */
		if (coff != 0 && match[0].rm_so >= coff)
			continue;

		/* Warn if wrapped. */
		if (wrapped && O_ISSET(sp, O_WARN) && LF_ISSET(SEARCH_MSG))
			search_msg(sp, S_WRAP);

#if defined(DEBUG) && 0
		TRACE(sp, "B found: %qu to %qu\n",
		    match[0].rm_so, match[0].rm_eo);
#endif
		/*
		 * We now have the first match on the line.  Step through the
		 * line character by character until find the last acceptable
		 * match.  This is painful, we need a better interface to regex
		 * to make this work.
		 */
		for (;;) {
			last = match[0].rm_so++;
			if (match[0].rm_so >= len)
				break;
			match[0].rm_eo = len;
			eval = regexec(&sp->sre, l, 1, match,
			    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) |
			    REG_STARTEND);
			if (eval == REG_NOMATCH)
				break;
			if (eval != 0) {
				re_error(sp, eval, &sp->sre);
				goto err;
			}
			if (coff && match[0].rm_so >= coff)
				break;
		}
		rm->lno = lno;

		/* See comment in f_search(). */
		if (!LF_ISSET(SEARCH_EOL) && last >= len)
			rm->cno = len != 0 ? len - 1 : 0;
		else
			rm->cno = last;
		rval = 0;
		break;
	}

err:	search_busy(sp, 0);
	return (rval);
}

/*
 * ctag_conv --
 *	Convert a tags search path into something that the POSIX
 *	1003.2 RE functions can handle.
 */
static int
ctag_conv(sp, ptrnp, replacedp)
	SCR *sp;
	char **ptrnp;
	int *replacedp;
{
	size_t blen, len;
	int lastdollar;
	char *bp, *p, *t;

	*replacedp = 0;

	len = strlen(p = *ptrnp);

	/* Max memory usage is 2 times the length of the string. */
	GET_SPACE_RET(sp, bp, blen, len * 2);

	t = bp;

	/* The last character is a '/' or '?', we just strip it. */
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
		else if (strchr("^.[]$*", p[0]))
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
 * re_conv --
 *	Convert vi's regular expressions into something that the
 *	the POSIX 1003.2 RE functions can handle.
 *
 * There are three conversions we make to make vi's RE's (specifically
 * the global, search, and substitute patterns) work with POSIX RE's.
 *
 * 1: If O_MAGIC is not set, strip backslashes from the magic character
 *    set (.[*~) that have them, and add them to the ones that don't.
 * 2: If O_MAGIC is not set, the string "\~" is replaced with the text
 *    from the last substitute command's replacement string.  If O_MAGIC
 *    is set, it's the string "~".
 * 3: The pattern \<ptrn\> does "word" searches, convert it to use the
 *    new RE escapes.
 *
 * !!!/XXX
 * This doesn't exactly match the historic behavior of vi because we do
 * the ~ substitution before calling the RE engine, so magic characters
 * in the replacement string will be expanded by the RE engine, and they
 * weren't historically.  It's a bug.
 *
 * PUBLIC: int re_conv __P((SCR *, char **, int *));
 */
int
re_conv(sp, ptrnp, replacedp)
	SCR *sp;
	char **ptrnp;
	int *replacedp;
{
	size_t blen, needlen;
	int magic;
	char *bp, *p, *t;

	/*
	 * First pass through, we figure out how much space we'll need.
	 * We do it in two passes, on the grounds that most of the time
	 * the user is doing a search and won't have magic characters.
	 * That way we can skip the malloc and memmove's.
	 */
	for (p = *ptrnp, magic = 0, needlen = 0; *p != '\0'; ++p)
		switch (*p) {
		case '\\':
			switch (*++p) {
			case '<':
				magic = 1;
				needlen += sizeof(RE_WSTART);
				break;
			case '>':
				magic = 1;
				needlen += sizeof(RE_WSTOP);
				break;
			case '~':
				if (!O_ISSET(sp, O_MAGIC)) {
					magic = 1;
					needlen += sp->repl_len;
				}
				break;
			case '.':
			case '[':
			case '*':
				if (!O_ISSET(sp, O_MAGIC)) {
					magic = 1;
					needlen += 1;
				}
				break;
			default:
				needlen += 2;
			}
			break;
		case '~':
			if (O_ISSET(sp, O_MAGIC)) {
				magic = 1;
				needlen += sp->repl_len;
			}
			break;
		case '.':
		case '[':
		case '*':
			if (!O_ISSET(sp, O_MAGIC)) {
				magic = 1;
				needlen += 2;
			}
			break;
		default:
			needlen += 1;
			break;
		}

	if (!magic) {
		*replacedp = 0;
		return (0);
	}

	/*
	 * Get enough memory to hold the final pattern.
	 *
	 * XXX
	 * It's nul-terminated, for now.
	 */
	GET_SPACE_RET(sp, bp, blen, needlen + 1);

	for (p = *ptrnp, t = bp; *p != '\0'; ++p)
		switch (*p) {
		case '\\':
			switch (*++p) {
			case '<':
				memmove(t, RE_WSTART, sizeof(RE_WSTART) - 1);
				t += sizeof(RE_WSTART) - 1;
				break;
			case '>':
				memmove(t, RE_WSTOP, sizeof(RE_WSTOP) - 1);
				t += sizeof(RE_WSTOP) - 1;
				break;
			case '~':
				if (O_ISSET(sp, O_MAGIC))
					*t++ = '~';
				else {
					memmove(t, sp->repl, sp->repl_len);
					t += sp->repl_len;
				}
				break;
			case '.':
			case '[':
			case '*':
				if (O_ISSET(sp, O_MAGIC))
					*t++ = '\\';
				*t++ = *p;
				break;
			default:
				*t++ = '\\';
				*t++ = *p;
			}
			break;
		case '~':
			if (O_ISSET(sp, O_MAGIC)) {
				memmove(t, sp->repl, sp->repl_len);
				t += sp->repl_len;
			} else
				*t++ = '~';
			break;
		case '.':
		case '[':
		case '*':
			if (!O_ISSET(sp, O_MAGIC))
				*t++ = '\\';
			*t++ = *p;
			break;
		default:
			*t++ = *p;
			break;
		}
	*t = '\0';

	*ptrnp = bp;
	*replacedp = 1;
	return (0);
}

/*
 * re_error --
 *	Report a regular expression error.
 *
 * PUBLIC: void re_error __P((SCR *, int, regex_t *));
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
		msgq(sp, M_SYSERR, NULL);
	else {
		(void)regerror(errcode, preg, oe, s);
		msgq(sp, M_ERR, "RE error: %s", oe);
	}
	free(oe);
}

/*
 * search_msg --
 *	Display one of the search messages.
 */
static void
search_msg(sp, msg)
	SCR *sp;
	smsg_t msg;
{
	switch (msg) {
	case S_EMPTY:
		msgq(sp, M_INFO, "072|File empty; nothing to search");
		break;
	case S_EOF:
		msgq(sp, M_INFO,
		    "073|Reached end-of-file without finding the pattern");
		break;
	case S_NOPREV:
		msgq(sp, M_ERR, "074|No previous search pattern");
		break;
	case S_NOTFOUND:
		msgq(sp, M_INFO, "075|Pattern not found");
		break;
	case S_SOF:
		msgq(sp, M_INFO,
		    "076|Reached top-of-file without finding the pattern");
		break;
	case S_WRAP:
		msgq(sp, M_VINFO, "077|Search wrapped");
		break;
	default:
		abort();
	}
}

/*
 * search_busy --
 *	Put up the busy searching message.
 *
 * PUBLIC: void search_busy __P((SCR *, int));
 */
void
search_busy(sp, on)
	SCR *sp;
	int on;
{
	sp->gp->scr_busy(sp, "078|Searching...", on);
}
