/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 8.7 1993/08/28 10:30:04 bostic Exp $ (Berkeley) $Date: 1993/08/28 10:30:04 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)

/*
 * file_argv --
 *	Do file name and shell expansion on a string, then
 *	break it up into an argv.
 */
int
file_argv(sp, ep, s, argcp, argvp)
	SCR *sp;
	EXF *ep;
	char *s, ***argvp;
	int *argcp;
{
	size_t len, blen, tlen;
	int rval;
	char *bp, *p, *t;

	GET_SPACE(sp, bp, blen, 512);

	memmove(bp, SHELLECHO, SHELLOFFSET);
	p = bp + SHELLOFFSET;
	len = SHELLOFFSET;

#if DEBUG && 0
	TRACE(sp, "file_argv: {%s}\n", s);
#endif

	/* Replace file name characters. */
	for (; *s; ++s)
		switch (*s) {
		case '%':
			if (F_ISSET(sp->frp, FR_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			ADD_SPACE(sp, bp, blen, len + sp->frp->nlen);
			memmove(p, sp->frp->fname, sp->frp->nlen);
			p += sp->frp->nlen;
			len += sp->frp->nlen;
			break;
		case '#':
			if (sp->alt_fname != NULL)
				tlen = strlen(t = sp->alt_fname);
			else if (sp->p_frp != NULL &&
			     !F_ISSET(sp->p_frp, FR_NONAME)) {
				t = sp->p_frp->fname;
				tlen = sp->p_frp->nlen;
			} else {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
				return (1);
			}
			ADD_SPACE(sp, bp, blen, len + tlen);
			memmove(p, t, tlen);
			p += tlen;
			len += tlen;
			break;
		case '\\':
			/*
			 * QUOTING NOTE:
			 *
			 * Strip backslashes that protected the file
			 * expansion characters.
			 */
			if (s[1] == '%' || s[1] == '#')
				++s;
			/* FALLTHROUGH */
		default:
			ADD_SPACE(sp, bp, blen, len + 1);
			*p++ = *s;
			++len;
		}

	/* Nul termination. */
	ADD_SPACE(sp, bp, blen, len + 1);
	*p = '\0';

#if DEBUG && 0
	TRACE(sp, "pre-shell: {%s}\n", bp);
#endif
	/*
	 * Do shell word expansion -- it's very, very hard to figure out
	 * what magic characters the user's shell expects.  If it's not
	 * pure vanilla, don't even try.
	 */
	for (p = bp; *p; ++p)
		if (!isalnum(*p) && !isblank(*p) && *p != '/')
			break;
	if (*p) {
		if (ex_run_process(sp, bp, &len, bp, blen))
			return (1);
		p = bp;
	} else
		p = bp + SHELLOFFSET;

#if DEBUG && 0
	TRACE(sp, "post-shell: {%s}\n", bp);
#endif

	rval = word_argv(sp, ep, p, argcp, argvp);

	FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * word_argv --
 *	Build an argv from a string.
 */
int
word_argv(sp, ep, p, argcp, argvp)
	SCR *sp;
	EXF *ep;
	char *p, ***argvp;
	int *argcp;
{
	size_t len;
	int cnt, done, off;
	char *ap, *t;

	for (done = off = 0;; ) {
		/*
		 * ESCAPE CHARACTER NOTE:
		 *
		 * New argument; NULL terminate, skipping anything
		 * that's preceded by the user's quoting character.
		 */
		for (ap = p; p[0] != '\0'; ++p) {
			if (sp->special[p[0]] == K_VLNEXT && p[1])
				p += 2;
			if (isblank(p[0]))
				break;
		}
		if (*p)
			*p = '\0';
		else
			done = 1;

		/*
		 * Allocate more pointer space if necessary; leave a space
		 * for a trailing NULL.
		 */
		len = (p - ap);
#define	INCREMENT	20
		if (off + 2 >= sp->argscnt - 1) {
			sp->argscnt += cnt = MAX(INCREMENT, 2);
			if ((sp->args = realloc(sp->args,
			    sp->argscnt * sizeof(ARGS))) == NULL) {
				free(sp->argv);
				goto mem1;
			}
			if ((sp->argv = realloc(sp->argv,
			    sp->argscnt * sizeof(char *))) == NULL) {
				free(sp->args);
mem1:				sp->argscnt = 0;
				sp->args = NULL;
				sp->argv = NULL;
				msgq(sp, M_ERR, "Error: %s.", strerror(errno));
				return (1);
			}
			memset(&sp->args[off], 0, cnt * sizeof(ARGS));
		}

		/*
		 * Copy the argument(s) into place, allocating space if
		 * necessary.
		 */
		if (sp->args[off].len < len && (sp->args[off].bp =
		    realloc(sp->args[off].bp, len)) == NULL) {
			sp->args[off].bp = NULL;
			sp->args[off].len = 0;
			msgq(sp, M_ERR, "Error: %s.", strerror(errno));
			return (1);
		}
		sp->argv[off] = sp->args[off].bp;
		sp->args[off].flags |= A_ALLOCATED;

		/*
		 * ESCAPE CHARACTER NOTE:
		 *
		 * Copy the argument into place, losing quote chars.
		 */
		for (t = sp->args[off].bp; len; *t++ = *ap++, --len)
			if (sp->special[*ap] == K_VLNEXT && len) {
				++ap;
				--len;
			}
		++off;

		if (done)
			break;

		/* Skip whitespace. */
		for (++p; isblank(p[0]); ++p);
		if (!*p)
			break;
	}
	sp->argv[off] = NULL;
	*argvp = sp->argv;
	*argcp = off;

#if DEBUG && 0
	for (cnt = 0; cnt < off; ++cnt)
		TRACE(sp, "arg %d: {%s}\n", cnt, sp->argv[cnt]);
#endif
	return (0);
}
