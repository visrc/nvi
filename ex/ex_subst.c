/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_subst.c,v 5.16 1992/11/01 15:03:48 bostic Exp $ (Berkeley) $Date: 1992/11/01 15:03:48 $";
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "options.h"
#include "search.h"
#include "pathnames.h"
#include "extern.h"

enum which {AGAIN, MUSTSETR, FIRST};

static int	regsub __P((u_char *, u_char *));
static int	substitute __P((EXCMDARG *, u_char *, regex_t *, enum which));

static regex_t sre;			/* Saved re. */
static int sre_set;			/* If saved re set. */
static regmatch_t *match;		/* Match table. */
static size_t matchnsub;		/* Match table size. */
static u_char *repl;			/* Replacement string. */

int
ex_substitute(cmdp)
	EXCMDARG *cmdp;
{
	regex_t *re, lre;
	int eval, reflags;
	u_char *ep, *l;
	char delim[2];

	/*
	 * Historic vi only permitted '/' to begin the substitution command.
	 * We permit ';' as well, since users often want to operate on UNIX
	 * pathnames.  We don't allow anything else because the 'r' flag
	 * character won't work.
	 */
	if (*cmdp->string == '/' || *cmdp->string == ';') {
		/* Set RE flags. */
		reflags = 0;
		if (ISSET(O_EXTENDED))
			reflags |= REG_EXTENDED;
		if (ISSET(O_IGNORECASE))
			reflags |= REG_ICASE;

		/* Delimiter is the first character. */
		delim[0] = cmdp->string[0];
		delim[1] = '\0';

		/* Get the substitute string. */
		ep = cmdp->string + 1;
		l = USTRSEP(&ep, delim);

		/* Get the replacement string, save it off. */
		if (*ep == NULL) {
			msg("No replacement string specified.");
			return (1);
		}
		if (repl != NULL)
			free(repl);
		repl = USTRSEP(&ep, delim);
		repl = USTRDUP(repl);

		/* If the substitute string is empty, use the last one. */
		if (*l == NULL) {
			if (!sre_set) {
				msg("No previous regular expression.");
				return (1);
			}
			return (substitute(cmdp, ep, &sre, AGAIN));
		}

		/* Compile the RE. */
		re = &lre;
		if (eval = regcomp(re, (char *)l, reflags)) {
			re_error(eval, re);
			return (1);
		}

		/* Set saved RE. */
		sre_set = 1;
		sre = lre;

		/* Build nsub structure as necessary. */
		if (matchnsub < re->re_nsub + 1) {
			matchnsub = re->re_nsub + 1;
			if ((match = realloc(match,
			    matchnsub * sizeof(regmatch_t))) == NULL) {
				msg("Error: %s", strerror(errno));
				match = NULL;
				matchnsub = 0;
				return (1);
			}
		}
		return (substitute(cmdp, ep, re, FIRST));
	}
	return (substitute(cmdp, cmdp->string, &sre, MUSTSETR));
}

int
ex_subagain(cmdp)
	EXCMDARG *cmdp;
{
	if (!sre_set) {
		msg("No previous regular expression.");
		return (1);
	}
	substitute(cmdp, cmdp->string, &sre, AGAIN);
	return (0);
}

static u_char *lb;				/* Build buffer. */
static size_t lbclen, lblen;			/* Current and total length. */

#define	NEEDSP(len, pnt) {						\
	if (lbclen + (len) > lblen) {					\
		lblen += MIN(lbclen + (len), 256);			\
		if ((lb = realloc(lb, lblen)) == NULL) {		\
			msg("Error: %s", strerror(errno));		\
			lbclen = 0;					\
			return (1);					\
		}							\
		pnt = lb + lbclen;					\
	}								\
}

#define	BUILD(l, len) {							\
	if (lbclen + (len) > lblen) {					\
		lblen += MIN(lbclen + (len), 256);			\
		if ((lb = realloc(lb, lblen)) == NULL) {		\
			msg("Error: %s", strerror(errno));		\
			lbclen = 0;					\
			return (1);					\
		}							\
	}								\
	memmove(lb + lbclen, l, len);					\
	lbclen += len;							\
}

static int
substitute(cmdp, s, re, cmd)
	EXCMDARG *cmdp;
	u_char *s;
	regex_t *re;
	enum which cmd;
{
	MARK from, to;
	recno_t elno, lno;
	size_t len, re_off;
	int eval, cflag, gflag, lflag, nflag, pflag, rflag;

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
			if (!sre_set) {
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
	curf->rptlines = 0;
	curf->rptlabel = "changed";
	for (lno = cmdp->addr1.lno, elno = cmdp->addr2.lno;
	    lno <= elno; ++lno) {

		/* Get the line. */
		if ((s = file_gline(curf, lno, &len)) == NULL) {
			GETLINE_ERR(lno);
			return (1);
		}

		/* Search the entire line. */
		match[0].rm_so = 0;
		match[0].rm_eo = len;
		eval = regexec(re,
		    (char *)s, re->re_nsub + 1, match, REG_STARTEND);
		if (eval == REG_NOMATCH)
			continue;
		if (eval != 0) {
			re_error(eval, re);
			return (1);
		}

		/* Reset new line buffer. */
		lbclen = 0;

		/* Do replacement. */
		if (gflag) {
			do {
				if (cflag) {
					from.lno = to.lno = lno;
					from.cno = match[0].rm_so;
					to.cno = match[0].rm_eo;
					if (!curf->s_confirm(curf, &from, &to))
						continue;
				}

				/* Locate start of replaced string. */
				re_off = match[0].rm_so;

				/* Copy leading retained string. */
				BUILD(s, re_off);

				/* Add in regular expression. */
				if (regsub(s, repl))
					return (1);

				/* Move past this match. */
				s += match[0].rm_eo;
				len -= match[0].rm_eo;
				match[0].rm_so = 0;
				match[0].rm_eo = len;
			} while (len &&
			    (eval = regexec(re, (char *)s, re->re_nsub + 1,
			    match, REG_NOTBOL | REG_STARTEND)) == 0);

			/* Check for an error. */
			if (len && eval != REG_NOMATCH) {
				re_error(eval, re);
				return (1);
			}

			/* Copy trailing retained string. */
			if (len)
				BUILD(s, len)
		} else {
			if (cflag) {
				from.lno = to.lno = lno;
				from.cno = match[0].rm_so;
				to.cno = match[0].rm_eo;
				if (!curf->s_confirm(curf, &from, &to))
					continue;
			}

			/* Locate start of replaced string. */
			re_off = match[0].rm_so;

			/* Copy leading retained string. */
			BUILD(s, re_off);

			/* Add in regular expression. */
			if (regsub(s, repl))
				return (1);

			/* Copy trailing retained string. */
			s += match[0].rm_eo;
			len -= match[0].rm_eo;
			if (len)
				BUILD(s, len);
		}

		if (lbclen == 0)
			continue;

		++curf->rptlines;

		/* Reset the line. */
		if (file_sline(curf, lno, lb, lbclen))
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
	 * Note if nothing found.  Otherwise, if nothing displayed to the
	 * screen, put something up.
	 */
	if (curf->rptlines == 0)
		msg("No match found.");
	else if (!lflag && !nflag && !pflag)
		autoprint = 1;

	return (0);
}

static int
regsub(string, src)
	u_char *string, *src;
{
	size_t len;
	int ch, no;
	u_char *dst;

	for (dst = lb + lbclen; ch = *src++;)
		if (ch == '&') {
			no = 0;
			goto sub;
		} else if (ch == '\\' && isdigit(*src)) {
			no = *src++ - '0';
sub:			if (match[no].rm_so != -1 && match[no].rm_eo != -1) {
				len = match[no].rm_eo - match[no].rm_so;
				NEEDSP(len, dst);
				memmove(dst, string + match[no].rm_so, len);
				dst += len;
				lbclen += len;
			}
		} else {			/* Ordinary character. */
 			if (ch == '\\' && (*src == '\\' || *src == '&'))
 				ch = *src++;
			NEEDSP(1, dst);
 			*dst++ = ch;
			++lbclen;
		}
	return (0);
}
