/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 5.11 1993/05/12 11:16:29 bostic Exp $ (Berkeley) $Date: 1993/05/12 11:16:29 $";
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
	GS *gp;
	int ch, len;
	char *p, *t, *gs, *tpath;

	/* Discard any previous information. */
	if (sp->g.gl_pathc)
		globfree(&sp->g);

	/* Figure out how much space we need for this argument. */
	for (p = word, len = 0; *p; ++p)
		switch (*p) {
		case '$':
			for (t = p + 1; *t && !isspace(*t); ++t);
			ch = *t;
			*t = '\0';
			if ((gs = getenv(p + 1)) == NULL) {
				if (p + 1 == t)
					msgq(sp, M_ERR,
	    "'$' indicates an environmental variable; use \\$ to escape it.");
				else
					msgq(sp, M_ERR,
					    "No environmental variable %s.",
					    p + 1);
				    return (1);
			}
			*t = ch;
			p = t;
			len += strlen(gs);
			break;
		case '%':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERR,
				    "No filename to substitute for %%.");
				return (1);
			}
			len += ep->nlen;
			break;
		case '#':
			if (sp->altfname != NULL)
				len += strlen(sp->altfname);
			else if (sp->eprev != NULL &&
			     !F_ISSET(sp->eprev, F_NONAME))
				len += sp->eprev->nlen;
			else {
				msgq(sp, M_ERR,
				    "No filename to substitute for #.");
				return (1);
			}
			break;
		case '\\':
			if (p[1] != '\0')
				++p;
			/* FALLTHROUGH */
		default:
			++len;
		}

	gp = sp->gp;
	if (len != wordlen) {
		/*
		 * Copy argument into temporary space, replacing file
		 * names.  Allocate temporary space if necessary.
		 */
		if (F_ISSET(gp, G_TMP_INUSE)) {
			if ((tpath = malloc(len + 1)) == NULL) {
				msgq(sp, M_ERR,
				    "Error: %s.", strerror(errno));
				return (1);
			}
		} else {
			BINC(sp, gp->tmp_bp, gp->tmp_blen, len + 1);
			tpath = gp->tmp_bp;
			F_SET(gp, G_TMP_INUSE);
		}

		for (p = tpath; *word; ++word)
			switch (*word) {
			case '$':
				for (t = word + 1; *t && !isspace(*t); ++t);
				ch = *t;
				*t = '\0';
				len = strlen(gs = getenv(word + 1));
				memmove(p, gs, len);
				*t = ch;
				word = t;
				p += len;
				break;
			case '%':
				memmove(p, ep->name, ep->nlen);
				p += ep->nlen;
				break;
			case '#':
				if (sp->altfname != NULL) {
					len = strlen(sp->altfname);
					memmove(p, sp->altfname, len);
				} else {
					len = sp->eprev->nlen;
					memmove(p, sp->eprev->name, len);
				}
				p += len;
				break;
			case '\\':
				if (p[1] != '\0')
					++p;
				/* FALLTHROUGH */
			default:
				*p++ = *word;
			}
		*p = '\0';
		p = tpath;
	} else {
		tpath = NULL;
		p = word;
	}

	/*
	 * XXX: GLOB_NOMAGIC:
	 * Then error if no pattern matches and magic char included.
	 */
	glob(p,
	    GLOB_NOCHECK | GLOB_NOSORT | GLOB_QUOTE | GLOB_TILDE, NULL, globp);

	if (tpath != NULL)
		if (tpath == gp->tmp_bp)
			F_CLR(gp, G_TMP_INUSE);
		else
			free(tpath);
	return (0);
}
