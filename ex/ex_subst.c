/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_subst.c,v 5.23 1993/02/12 09:29:39 bostic Exp $ (Berkeley) $Date: 1993/02/12 09:29:39 $";
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "search.h"
#include "term.h"
#include "pathnames.h"

enum which {AGAIN, MUSTSETR, FIRST};

static int	checkmatchsize __P((regex_t *));
static inline int	regsub __P((u_char *));
static int	substitute __P((EXCMDARG *, u_char *, regex_t *, enum which));

static regmatch_t *match;		/* Match array. */
static size_t matchsize;		/* Match array size. */
static u_char *repl;			/* Replacement string. */
static size_t lrepl;			/* Replacement string length. */

int
ex_substitute(cmdp)
	EXCMDARG *cmdp;
{
	regex_t *re, lre;
	int eval, reflags;
	u_char *ep, *sub;
	char delim[2];

	/*
	 * Historic vi only permitted '/' to begin the substitution command.
	 * We permit ';' as well, since users often want to operate on UNIX
	 * pathnames.  We don't allow just anything because the flag chars
	 * wouldn't work.
	 */
	if (*cmdp->string == '/' || *cmdp->string == ';') {
		/* Delimiter is the first character. */
		delim[0] = cmdp->string[0];
		delim[1] = '\0';

		/* Get the substitute string. */
		ep = cmdp->string + 1;
		sub = USTRSEP(&ep, delim);

		/* Get the replacement string, save it off. */
		if (*ep == NULL) {
			msg("No replacement string specified.");
			return (1);
		}
		if (repl != NULL)
			free(repl);
		repl = USTRSEP(&ep, delim);	/* XXX Not 8-bit clean. */
		repl = USTRDUP(repl);
		lrepl = USTRLEN(repl);

		/* If the substitute string is empty, use the last one. */
		if (*sub == NULL) {
			if (!FF_ISSET(curf, F_RE_SET)) {
				msg("No previous regular expression.");
				return (1);
			}
			if (checkmatchsize(&curf->sre))
				return (1);
			return (substitute(cmdp, ep, &curf->sre, AGAIN));
		}

		/* Set RE flags. */
		reflags = 0;
		if (ISSET(O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (ISSET(O_IGNORECASE))
			reflags |= REG_ICASE;

		/* Compile the RE. */
		re = &lre;
		if (eval = regcomp(re, (char *)sub, reflags)) {
			re_error(eval, re);
			return (1);
		}

		/* Set saved RE. */
		curf->sre = lre;
		FF_SET(curf, F_RE_SET);

		if (checkmatchsize(&curf->sre))
			return (1);
		return (substitute(cmdp, ep, re, FIRST));
	}
	return (substitute(cmdp, cmdp->string, &curf->sre, MUSTSETR));
}

int
ex_subagain(cmdp)
	EXCMDARG *cmdp;
{
	if (FF_ISSET(curf, F_RE_SET)) {
		msg("No previous regular expression.");
		return (1);
	}
	return (substitute(cmdp, cmdp->string, &curf->sre, AGAIN));
}

static u_char *lb;			/* Build buffer. */
static size_t lbclen, lblen;		/* Current and total length. */

/* 
 * The nasty part of the substitution is what happens when the replacement
 * string contains newlines.  It's a bit tricky -- consider the information
 * that has to be retained for "s/f\(o\)o/^M\1^M\1/".  The solution here is
 * to build a set of newline offets which we use to break the line up later,
 * when the replacement is done.  Don't change it unless you're pretty damned
 * confident.
 */
static size_t *newl;			/* Newline array. */
static size_t newlsize;			/* Newline array size. */
static size_t newlcnt;			/* Newlines in replacement. */

#define	BUILD(l, len) {							\
	if (lbclen + (len) > lblen) {					\
		lblen += MAX(lbclen + (len), 256);			\
		if ((lb = realloc(lb, lblen)) == NULL) {		\
			msg("Error: %s", strerror(errno));		\
			lbclen = 0;					\
			return (1);					\
		}							\
	}								\
	memmove(lb + lbclen, l, len);					\
	lbclen += len;							\
}

#define	NEEDNEWLINE {							\
	if (newlsize == newlcnt) {					\
		newlsize += 25;						\
		if ((newl = realloc(newl,				\
		    newlsize * sizeof(size_t))) == NULL) {		\
			msg("Error: %s", strerror(errno));		\
			newlsize = 0;					\
			return (1);					\
		}							\
	}								\
}

#define	NEEDSP(len, pnt) {						\
	if (lbclen + (len) > lblen) {					\
		lblen += MAX(lbclen + (len), 256);			\
		if ((lb = realloc(lb, lblen)) == NULL) {		\
			msg("Error: %s", strerror(errno));		\
			lbclen = 0;					\
			return (1);					\
		}							\
		pnt = lb + lbclen;					\
	}								\
}

static int
substitute(cmdp, s, re, cmd)
	EXCMDARG *cmdp;
	u_char *s;
	regex_t *re;
	enum which cmd;
{
	MARK from, to;
	recno_t elno, lno, lastline;
	size_t cnt, last, len, offset;
	int eflags, eval;
	int cflag, gflag, lflag, nflag, pflag, rflag;

	/*
	 * Historic vi permitted the '#', 'l' and 'p' options in vi mode,
	 * but it only displayed the last change and they really don't
	 * make any sense.  In the current model the problem is combining
	 * them with the 'c' flag -- the screen would have to flip back
	 * and forth between the confirm screen and the ex print screen,
	 * which would be pretty awful.
	 */
	cflag = gflag = lflag = nflag = pflag = rflag = 0;
	for (; *s; ++s)
		switch(*s) {
		case ' ':
		case '\t':
			break;
		case '#':
			if (mode == MODE_VI) {
				msg("'#' flag not supported in vi mode.");
				return (1);
			}
			nflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'l':
			if (mode == MODE_VI) {
				msg("'l' flag not supported in vi mode.");
				return (1);
			}
			lflag = 1;
			break;
		case 'p':
			if (mode == MODE_VI) {
				msg("'p' flag not supported in vi mode.");
				return (1);
			}
			pflag = 1;
			break;
		case 'r':
			if (cmd == FIRST) {
		msg("Regular expression specified; r flag meaningless.");
				return (1);
			}
			if (!FF_ISSET(curf, F_RE_SET)) {
				msg("No previous regular expression.");
				return (1);
			}
			rflag = 1;
			break;
		default:
			goto usage;
		}

	if (rflag == 0 && cmd == MUSTSETR) {
usage:		msg("Usage: %s", cmdp->cmd->usage);
		return (1);
	}

	/* For each line... */
	lastline = OOBLNO;
	curf->rptlines = 0;
	curf->rptlabel = "changed";
	for (lno = cmdp->addr1.lno,
	    elno = cmdp->addr2.lno; lno <= elno; ++lno) {

		/* Get the line. */
		if ((s = file_gline(curf, lno, &len)) == NULL) {
			GETLINE_ERR(lno);
			return (1);
		}

		lbclen = 0;
		eflags = REG_STARTEND;

		/* Search for a match. */
nextmatch:	match[0].rm_so = 0;
		match[0].rm_eo = len;

skipmatch:	eval = regexec(re, (char *)s, re->re_nsub + 1, match, eflags);
		if (eval == REG_NOMATCH) {
			if (lbclen != 0)
				goto nomatch;
			continue;
		}
		if (eval != 0) {
			re_error(eval, re);
			return (1);
		}

		/* Confirm change. */
		if (cflag) {
			from.lno = lno;
			from.cno = match[0].rm_so;
			to.lno = lno;
			to.cno = match[0].rm_eo;

			switch(curf->s_confirm(curf, &from, &to)) {
			case YES:
				break;
			case NO:
				/* Copy prefix, matching bytes. */
				BUILD(s, match[0].rm_eo);
				goto skip;
			case QUIT:
				elno = lno;
				goto nomatch;
			}
		}

		/* Copy prefix. */
		BUILD(s, match[0].rm_so);

		/* Copy matching bytes. */
		if (regsub(s))
			return (1);

skip:		s += match[0].rm_eo;
		len -= match[0].rm_eo;

		/*
		 * We have to update the screen.  The basic idea is to store
		 * the line so the screen update routines can find it, but
		 * start at the old offset.
		 */
		if (len && lbclen && gflag && cflag) {
			/* Save offset. */
			offset = lbclen;
			
			/* Copy suffix. */
			if (len)
				BUILD(s, len)

			/* Store any inserted lines. */
			last = 0;
			if (newlcnt) {
				for (cnt = 0;
				    cnt < newlcnt; ++cnt, ++lno, ++elno) {
					if (file_iline(curf,
					    lno, lb + last, newl[cnt] - last))
						return (1);
					last = newl[cnt] + 1;
				}
				lbclen -= last;
				offset -= last;
				newlcnt = 0;
			}

			/* Store the line. */
			if (lbclen)
				if (file_sline(curf, lno, lb + last, lbclen))
					return (1);

			/* Get a new copy of the line. */
			if ((s = file_gline(curf, lno, &len)) == NULL) {
				GETLINE_ERR(lno);
				return (1);
			}

			/* Restart the buffer. */
			lbclen = 0;

			/* Update counters. */
			if (lastline != lno) {
				++curf->rptlines;
				lastline = lno;
			}

			/* Start in the middle of the line. */
			match[0].rm_so = offset;
			match[0].rm_eo = len;
			goto skipmatch;
		}

		/* Get the next match. */
		if (len && gflag) {
			eflags |= REG_NOTBOL;
			goto nextmatch;
		}

		/* Copy suffix. */
nomatch:	if (len)
			BUILD(s, len)

		/* Update counters. */
		if (lastline != lno) {
			++curf->rptlines;
			lastline = lno;
		}

		/* Store any inserted lines. */
		last = 0;
		if (newlcnt) {
			for (cnt = 0; cnt < newlcnt; ++cnt, ++lno, ++elno) {
				if (file_iline(curf,
				    lno, lb + last, newl[cnt] - last))
					return (1);
				last = newl[cnt] + 1;
			}
			lbclen -= last;
			newlcnt = 0;
		}

		/* Store the line. */
		if (lbclen)
			if (file_sline(curf, lno, lb + last, lbclen))
				return (1);

		/* Display as necessary. */
		if (!lflag && !nflag && !pflag)
			continue;
		if (lflag)
			ex_print(curf, &from, &to, E_F_LIST);
		if (nflag)
			ex_print(curf, &from, &to, E_F_HASH);
		if (pflag)
			ex_print(curf, &from, &to, E_F_PRINT);
	}

	/*
	 * Note if nothing found.  Else, if nothing displayed to the
	 * screen, put something up.
	 */
	if (curf->rptlines == 0 && !FF_ISSET(curf, F_IN_GLOBAL))
		msg("No match found.");
	else if (!lflag && !nflag && !pflag)
		FF_SET(curf, F_AUTOPRINT);

	return (0);
}

/*
 * regsub --
 * 	Do the substitution for a regular expression.
 */
static inline int
regsub(ip)
	u_char *ip;			/* Input line. */
{
	size_t mlen;			/* Match length. */
	size_t rpl;			/* Remaining replacement length. */
	u_char *rp;			/* Replacement pointer. */
	int ch;
	int no;				/* Match replacement offset. */
	u_char *lbp;			/* Build buffer pointer. */

	rp = repl;			/* Set up replacment info. */
	rpl = lrepl;
	for (lbp = lb + lbclen; rpl--;) {
		ch = *rp++;
		if (ch == '&') {	/* Entire pattern. */
			no = 0;
			goto sub;
					/* Partial pattern. */
		} else if (ch == '\\' && isdigit(*rp)) {
			no = *rp++ - '0';
sub:			if (match[no].rm_so != -1 && match[no].rm_eo != -1) {
				mlen = match[no].rm_eo - match[no].rm_so;
				NEEDSP(mlen, lbp);
				memmove(lbp, ip + match[no].rm_so, mlen);
				lbp += mlen;
				lbclen += mlen;
			}
		} else {		/* Newline, ordinary characters. */
			if (special[ch] == K_CR || special[ch] == K_NL) {
				NEEDNEWLINE;
				newl[newlcnt++] = lbclen;
			} else if (ch == '\\' && (*rp == '\\' || *rp == '&'))
 				ch = *rp++;
			NEEDSP(1, lbp);
 			*lbp++ = ch;
			++lbclen;
		}
	}
	return (0);
}

static int
checkmatchsize(re)
	regex_t *re;
{
	/* Build nsub array as necessary. */
	if (matchsize < re->re_nsub + 1) {
		matchsize = re->re_nsub + 1;
		if ((match =
		    realloc(match, matchsize * sizeof(regmatch_t))) == NULL) {
			msg("Error: %s", strerror(errno));
			matchsize = 0;
			return (1);
		}
	}
	return (0);
}
