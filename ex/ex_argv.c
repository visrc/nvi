/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 5.13 1993/05/13 11:12:09 bostic Exp $ (Berkeley) $Date: 1993/05/13 11:12:09 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

static int fileexpand __P((SCR *, EXF *, glob_t *, char *, int));

/*
 * buildargv --
 *	Build an argv from a string.
 */
int
buildargv(sp, ep, s, expand, argcp, argvp)
	SCR *sp;
	EXF *ep;
	char *s, ***argvp;
	int expand, *argcp;
{
	int cnt, done, globoff, len, needslots, off;
	char *ap, *p;

	/* Discard any previous information. */
	if (sp->g.gl_pathc)
		globfree(&sp->g);

	/* Break into argv vector. */
	for (done = globoff = off = 0;; ) {
		/*
		 * New argument; NULL terminate, skipping anything that's
		 * preceded by a quoting character.
		 */
		for (ap = s; s[0]; ++s) {
			if (s[0] == '\\' && s[1])
				s += 2;
			if (isspace(s[0]))
				break;
		}
		if (*s)
			*s = '\0';
		else
			done = 1;

		/*
		 * Expand and count up the necessary slots.  Add +1 to
		 * include the trailing NULL.
		 */
		len = s - ap +1;

		if (expand) {
			if (fileexpand(sp, ep, &sp->g, ap, len))
				return (1);
			needslots = sp->g.gl_pathc - globoff + 1;
		} else
			needslots = 2;

		/*
		 * Allocate more pointer space if necessary; leave a space
		 * for a trailing NULL.
		 */
#define	INCREMENT	20
		if (off + needslots >= sp->argscnt - 1) {
			sp->argscnt += cnt = MAX(INCREMENT, needslots);
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
				msgq(sp, M_ERR,
				    "Error: %s.", strerror(errno));
				return (1);
			}
			memset(&sp->args[off], 0, cnt * sizeof(ARGS));
		}

		/*
		 * Copy the argument(s) into place, allocating space if
		 * necessary.
		 */
		if (expand) {
			for (cnt = globoff;
			    cnt < sp->g.gl_pathc; ++cnt, ++off) {
				if (sp->args[off].flags & A_ALLOCATED) {
					free(sp->args[off].bp);
					sp->args[off].flags &= ~A_ALLOCATED;
				}
				sp->argv[off] =
				    sp->args[off].bp = sp->g.gl_pathv[cnt];
			}
			globoff = sp->g.gl_pathc;
		} else {
			if (sp->args[off].len < len && (sp->args[off].bp =
			    realloc(sp->args[off].bp, len)) == NULL) {
				sp->args[off].bp = NULL;
				sp->args[off].len = 0;
				msgq(sp, M_ERR,
				    "Error: %s.", strerror(errno));
				return (1);
			}
			sp->argv[off] = sp->args[off].bp;
			sp->args[off].flags |= A_ALLOCATED;
			/* Copy the argument into place, losing quote chars. */
			for (p = sp->args[off].bp; len; *p++ = *ap++, --len)
				if (*ap == '\\' && len) {
					++ap;
					--len;
				}
			++off;
		}

		if (done)
			break;

		/* Skip whitespace. */
		while (isspace(*++s));
		if (!*s)
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

/*
 * fileexpand --
 *	Expand file names.
 */
static int
fileexpand(sp, ep, globp, word, wordlen)
	SCR *sp;
	EXF *ep;
	glob_t *globp;
	char *word;
	int wordlen;
{
	size_t blen, len, tlen;
	char *bp, *p, *t;

	GET_SPACE(sp, bp, blen, 512);

	/* It's going to become a shell command. */
	p = bp;
	memmove(p, "echo ", sizeof("echo ") - 1);
	p += sizeof("echo ") - 1;
	len = sizeof("echo ") - 1;

	/* Replace file name characters. */
	for (; *word; ++word)
		switch (*word) {
		case '%':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			ADD_SPACE(sp, bp, blen, len + ep->nlen);
			memmove(p, ep->name, ep->nlen);
			p += ep->nlen;
			len += ep->nlen;
			break;
		case '#':
			if (sp->altfname != NULL)
				tlen = strlen(t = sp->altfname);
			else if (sp->eprev != NULL &&
			     !F_ISSET(sp->eprev, F_NONAME)) {
				t = sp->eprev->name;
				tlen = sp->eprev->nlen;
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
			if (p[1] != '\0')
				++p;
			/* FALLTHROUGH */
		default:
			ADD_SPACE(sp, bp, blen, len + 1);
			*p++ = *word;
			++len;
		}

	/* Nul termination. */
	ADD_SPACE(sp, bp, blen, len + 1);
	*p = '\0';

#if DEBUG && 0
	TRACE(sp, "file char replacement: {%.*s}\n", len, bp);
#endif
	/*
	 * Do shell word expansion -- it's very, very hard to figure out
	 * what magic characters the user's shell expects.  If it's not
	 * vanilla, don't even try.
	 */
	for (p = bp; *p; ++p)
		if (!isalnum(*p) && !isspace(*p) && *p != '/')
			break;
	if (*p) {
		if (ex_run_process(sp, bp, &len, bp, blen))
			return (1);
#if DEBUG && 0
		TRACE(sp, "shell word expansion: {%.*s}\n", len, bp);
#endif
		p = bp;
	} else
		p = bp + (sizeof("echo ") - 1);

	/*
	 * Do shell globbing.
	 * XXX: GLOB_NOMAGIC:
	 * Then error if no pattern matches and magic char included.
	 */
	glob(p,
	    GLOB_APPEND | GLOB_NOCHECK | GLOB_NOSORT | GLOB_QUOTE | GLOB_TILDE,
	    NULL, globp);

	FREE_SPACE(sp, bp, blen);
	return (0);
}
