/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 10.2 1995/05/05 18:43:53 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:43:53 $";
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

static int	ctag_conv __P((SCR *, char **, int *));

enum smsgtype { S_EMPTY, S_EOF, S_NOPREV, S_NOTFOUND, S_SOF, S_WRAP };
static void smsg __P((SCR *, enum smsgtype));

/*
 * srch_setup --
 *	Set up a search.
 *
 * PUBLIC: int srch_setup __P((SCR *, dir_t, char *, char **, u_int));
 */
int
srch_setup(sp, dir, ptrn, epp, flags)
	SCR *sp;
	dir_t dir;
	char *ptrn, **epp;
	u_int flags;
{
	recno_t lno;
	regex_t re;
	int delim, eval, re_flags, replaced;
	char *p, *t;

	/* Copy the flags to the saved flags. */
	FL_INIT(sp->srch_flags, SEARCH_FIRST | flags);

	/* If the file is empty, it's a fast search. */
	if (sp->lno <= 1) {
		if (file_lline(sp, &lno))
			return (1);
		if (lno == 0) {
			if (LF_ISSET(SEARCH_MSG))
				smsg(sp, S_EMPTY);
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
				smsg(sp, S_NOPREV);
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
					smsg(sp, S_NOPREV);
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

/* __TK__ DELETE FSRCH_SET && BSRCH_SETUP */
/*
 * fsrch_setup --
 *	Set up for a forward search.
 *
 * PUBLIC: int fsrch_setup __P((SCR *, MARK *, MARK *, char *, char **, u_int));
 */
int
fsrch_setup(sp, fm, rm, ptrn, eptrn, flags)
	SCR *sp;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int flags;
{
	if (srch_setup(sp, FORWARD, ptrn, eptrn, flags))
		return (1);

	/* Set up the search state. */
	FL_INIT(sp->srch_flags, SEARCH_FIRST | flags);
	sp->srch_fm = fm;
	sp->srch_rm = rm;
	FL_SET(sp->gp->ec_flags, EC_INTERRUPT);
	return (0);
}

/*
 * fsrch --
 *	Do a forward search.
 *
 * PUBLIC: int fsrch __P((SCR *, EVENT *, int *));
 */
int
fsrch(sp, evp, completep)
	SCR *sp;
	EVENT *evp;
	int *completep;
{
	regmatch_t match[1];
	size_t len;
	int cnt, eval;
	char *l;

	if (evp != NULL && evp->e_event == E_INTERRUPT)
		goto notfound;

	/*
	 * Start searching immediately after the cursor.  If at the end of the
	 * line, start searching on the next line.  This is incompatible (read
	 * bug fix) with the historic vi -- searches for the '$' pattern never
	 * moved forward, and "-t foo" didn't work if "foo" was the first thing
	 * in the file.
	 */
	if (FL_ISSET(sp->srch_flags, SEARCH_FIRST)) {
		if (FL_ISSET(sp->srch_flags, SEARCH_FILE)) {
			sp->srch_lno = 1;
			sp->srch_coff = 0;
		} else {
			if ((l = file_gline(sp,
			    sp->srch_fm->lno, &len)) == NULL) {
				FILE_LERR(sp, sp->srch_fm->lno);
				return (1);
			}
			if (sp->srch_fm->cno + 1 >= len) {
				if (sp->srch_fm->lno == sp->srch_lno) {
					if (!O_ISSET(sp, O_WRAPSCAN)) {
						if (FL_ISSET(sp->srch_flags,
						    SEARCH_MSG))
							smsg(sp, S_EOF);
						goto notfound;
					}
					sp->srch_lno = 1;
				} else
					sp->srch_lno = sp->srch_fm->lno + 1;
				sp->srch_coff = 0;
			} else {
				sp->srch_lno = sp->srch_fm->lno;
				sp->srch_coff = sp->srch_fm->cno + 1;
			}
		}

		FL_CLR(sp->srch_flags, SEARCH_FIRST);
	}

	for (cnt = INTERRUPT_CHECK;; ++sp->srch_lno, sp->srch_coff = 0) {
		if (cnt-- == 0) {
			if (!F_ISSET(sp, S_BUSY))
				srch_busy(sp, 1);
			*completep = 0;
			return (0);
		}
		if (FL_ISSET(sp->srch_flags, SEARCH_WRAPPED) &&
		    sp->srch_lno > sp->srch_fm->lno ||
		    (l = file_gline(sp, sp->srch_lno, &len)) == NULL) {
			if (FL_ISSET(sp->srch_flags, SEARCH_WRAPPED)) {
				if (FL_ISSET(sp->srch_flags, SEARCH_MSG))
					smsg(sp, S_NOTFOUND);
				goto notfound;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (FL_ISSET(sp->srch_flags, SEARCH_MSG))
					smsg(sp, S_EOF);
				goto notfound;
			}
			sp->srch_lno = 0;
			FL_SET(sp->srch_flags, SEARCH_WRAPPED);
			continue;
		}

		/* If already at EOL, just keep going. */
		if (len != 0 && sp->srch_coff == len)
			continue;

		/* Set the termination. */
		match[0].rm_so = sp->srch_coff;
		match[0].rm_eo = len;

#if defined(DEBUG) && 0
		TRACE(sp, "F search: %lu from %u to %u\n",
		    sp->srch_lno, sp->srch_coff, len != 0 ? len - 1 : len);
#endif
		/* Search the line. */
		eval = regexec(&sp->sre, l, 1, match,
		    (match[0].rm_so == 0 ? 0 : REG_NOTBOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, &sp->sre);
			goto notfound;
		}


		/* Warn if wrapped. */
		if (O_ISSET(sp, O_WARN) &&
		    FL_ISSET(sp->srch_flags, SEARCH_MSG) &&
		    FL_ISSET(sp->srch_flags, SEARCH_WRAPPED))
			smsg(sp, S_WRAP);

#if defined(DEBUG) && 0
		TRACE(sp, "fsrch: %qu to %qu\n",
		    match[0].rm_so, match[0].rm_eo);
#endif
		sp->srch_rm->lno = sp->srch_lno;
		sp->srch_rm->cno = match[0].rm_so;

		/*
		 * If a change command, it's possible to move beyond the end
		 * of a line.  Historic vi generally got this wrong (e.g. try
		 * "c?$<cr>").  Not all that sure this gets it right, there
		 * are lots of strange cases.
		 */
		if (!FL_ISSET(sp->srch_flags, SEARCH_EOL) &&
		    sp->srch_rm->cno >= len)
			sp->srch_rm->cno = len != 0 ? len - 1 : 0;

		*completep = 1;
		FL_SET(sp->srch_flags, SEARCH_FOUND);
		srch_busy(sp, 0);
		return (0);
	}

notfound:
	*completep = 1;
	FL_CLR(sp->srch_flags, SEARCH_FOUND);
	srch_busy(sp, 0);
	return (0);
}

/*
 * bsrch_setup --
 *	Set up for a backward search.
 *
 * PUBLIC: int bsrch_setup __P((SCR *, MARK *, MARK *, char *, char **, u_int));
 */
int
bsrch_setup(sp, fm, rm, ptrn, eptrn, flags)
	SCR *sp;
	MARK *fm, *rm;
	char *ptrn, **eptrn;
	u_int flags;
{
	if (srch_setup(sp, BACKWARD, ptrn, eptrn, flags))
		return (1);

	/* Set up the search state. */
	FL_SET(sp->srch_flags, SEARCH_FIRST | flags);
	sp->srch_fm = fm;
	sp->srch_rm = rm;
	FL_SET(sp->gp->ec_flags, EC_INTERRUPT);
	return (0);
}

/*
 * bsrch --
 *	Do a backward search.
 *
 * PUBLIC: int bsrch __P((SCR *, EVENT *, int *));
 */
int
bsrch(sp, evp, completep)
	SCR *sp;
	EVENT *evp;
	int *completep;
{
	regmatch_t match[1];
	size_t len, last;
	int cnt, eval;
	char *l;

	if (evp != NULL && evp->e_event == E_INTERRUPT)
		goto notfound;
	
	if (FL_ISSET(sp->srch_flags, SEARCH_FIRST)) {
		/* If in the first column, start search on the previous line. */
		if (sp->srch_fm->cno == 0) {
			if (sp->srch_fm->lno == 1) {
				if (!O_ISSET(sp, O_WRAPSCAN)) {
					if (FL_ISSET(sp->srch_flags,
					    SEARCH_MSG))
						smsg(sp, S_SOF);
					goto notfound;
				}
			} else
				sp->srch_lno = sp->srch_fm->lno - 1;
		} else
			sp->srch_lno = sp->srch_fm->lno;
		sp->srch_coff = sp->srch_fm->cno;

		FL_CLR(sp->srch_flags, SEARCH_FIRST);
	}

	for (cnt = INTERRUPT_CHECK;; --sp->srch_lno, sp->srch_coff = 0) {
		if (cnt-- == 0) {
			if (!F_ISSET(sp, S_BUSY))
				srch_busy(sp, 1);
			*completep = 0;
			return (0);
		}
		if (FL_ISSET(sp->srch_flags, SEARCH_WRAPPED) &&
		    sp->srch_lno < sp->srch_fm->lno || sp->srch_lno == 0) {
			if (FL_ISSET(sp->srch_flags, SEARCH_WRAPPED)) {
				if (FL_ISSET(sp->srch_flags, SEARCH_MSG))
					smsg(sp, S_NOTFOUND);
				goto notfound;
			}
			if (!O_ISSET(sp, O_WRAPSCAN)) {
				if (FL_ISSET(sp->srch_flags, SEARCH_MSG))
					smsg(sp, S_SOF);
				goto notfound;
			}
			if (file_lline(sp, &sp->srch_lno))
				return (1);
			if (sp->srch_lno == 0) {
				if (FL_ISSET(sp->srch_flags, SEARCH_MSG))
					smsg(sp, S_EMPTY);
				break;
			}
			++sp->srch_lno;
			FL_SET(sp->srch_flags, SEARCH_WRAPPED);
			continue;
		}

		if ((l = file_gline(sp, sp->srch_lno, &len)) == NULL)
			return (1);

		/* Set the termination. */
		match[0].rm_so = 0;
		match[0].rm_eo = len;

#if defined(DEBUG) && 0
		TRACE(sp, "bsrch: %lu from 0 to %qu\n",
		    sp->srch_lno, match[0].rm_eo);
#endif
		/* Search the line. */
		eval = regexec(&sp->sre, l, 1, match,
		    (match[0].rm_eo == len != 0 ?
		        0 : REG_NOTEOL) | REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(sp, eval, &sp->sre);
			goto notfound;
		}

		/* Check for a match starting past the cursor. */
		if (sp->srch_coff != 0 && match[0].rm_so >= sp->srch_coff)
			continue;

		/* Warn if wrapped. */
		if (O_ISSET(sp, O_WARN) &&
		    FL_ISSET(sp->srch_flags, SEARCH_MSG) &&
		    FL_ISSET(sp->srch_flags, SEARCH_WRAPPED))
			smsg(sp, S_WRAP);

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
				goto notfound;
			}
			if (sp->srch_coff && match[0].rm_so >= sp->srch_coff)
				break;
		}
		sp->srch_rm->lno = sp->srch_lno;

		/* See comment in fsrch(). */
		if (!FL_ISSET(sp->srch_flags, SEARCH_EOL) && last >= len)
			sp->srch_rm->cno = len != 0 ? len - 1 : 0;
		else
			sp->srch_rm->cno = last;

		*completep = 1;
		FL_SET(sp->srch_flags, SEARCH_FOUND);
		srch_busy(sp, 0);
		return (0);
	}
notfound:

	*completep = 1;
	FL_CLR(sp->srch_flags, SEARCH_FOUND);
	srch_busy(sp, 0);
	return (0);
}

/*
 * srch_busy --
 *	Put up a busy message for the searching routines.
 *
 * PUBLIC: void srch_busy __P((SCR *, int));
 */
void
srch_busy(sp, on)
	SCR *sp;
	int on;
{
	if (on) {
		F_SET(sp, S_BUSY);
		sp->gp->scr_busy(sp, msg_cat(sp, "280|Searching ...", NULL), 1);
	} else if (F_ISSET(sp, S_BUSY)) {
		F_CLR(sp, S_BUSY);
		sp->gp->scr_busy(sp, NULL, 0);
	}
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
 * smsg --
 *	Display one of the search messages.
 */
static void
smsg(sp, msg)
	SCR *sp;
	enum smsgtype msg;
{
	switch (msg) {
	case S_EMPTY:
		msgq(sp, M_INFO, "238|File empty; nothing to search");
		break;
	case S_EOF:
		msgq(sp, M_INFO,
		    "239|Reached end-of-file without finding the pattern");
		break;
	case S_NOPREV:
		msgq(sp, M_ERR, "243|No previous search pattern");
		break;
	case S_NOTFOUND:
		msgq(sp, M_INFO, "240|Pattern not found");
		break;
	case S_SOF:
		msgq(sp, M_INFO,
		    "241|Reached top-of-file without finding the pattern");
		break;
	case S_WRAP:
		msgq(sp, M_VINFO, "242|Search wrapped");
		break;
	default:
		abort();
	}
}
