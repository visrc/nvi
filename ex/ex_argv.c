/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_argv.c,v 5.2 1993/04/06 11:37:12 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:37:12 $";
#endif /* not lint */

#include <sys/param.h>

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
	int ch, cnt, done, globoff, len, needslots, off;
	char *ap;

	/* Discard any previous information. */
	if (sp->g.gl_pathc) {
		globfree(&sp->g);
		sp->g.gl_pathc = 0;
		sp->g.gl_offs = 0;
		sp->g.gl_pathv = NULL;
	}

	/* Break into argv vector. */
	for (done = globoff = off = 0;; ) {
		/* New argument; NULL terminate. */
		for (ap = s; *s && !isspace(*s); ++s);
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
				msgq(sp, M_ERROR,
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
				msgq(sp, M_ERROR,
				    "Error: %s.", strerror(errno));
				return (1);
			}
			sp->argv[off] = sp->args[off].bp;
			memmove(sp->args[off].bp, ap, len);
			sp->args[off].flags |= A_ALLOCATED;
			++off;
		}

		if (done)
			break;

		/* Skip whitespace. */
		while (ch = *++s) {
			if (ch == '\\' && s[1])
				++s;
			if (!isspace(ch))
				break;
		}
		if (!*s)
			break;
	}
	sp->argv[off] = NULL;
	*argvp = sp->argv;
	*argcp = off;
	return (0);
}

static int
fileexpand(sp, ep, globp, word, wordlen)
	SCR *sp;
	EXF *ep;
	glob_t *globp;
	char *word;
	int wordlen;
{
	EXF *prev_ep;
	GS *gp;
	int len;
	char *p, *tpath;

	/*
	 * Check for escaped %, # characters.
	 * XXX
	 */
	/* Figure out how much space we need for this argument. */
	prev_ep = NULL;
	len = wordlen;
	for (p = word; p = strpbrk(p, "%#\\"); ++p)
		switch (*p) {
		case '%':
			if (F_ISSET(ep, F_NONAME)) {
				msgq(sp, M_ERROR,
				    "No filename to substitute for %%.");
				return (1);
			}
			len += ep->nlen;
			break;
		case '#':
			if (prev_ep == NULL)
				prev_ep = file_prev(sp, ep, 0);
			if (prev_ep == NULL || F_ISSET(prev_ep, F_NONAME)) {
				msgq(sp, M_ERROR,
				    "No filename to substitute for #.");
				return (1);
			}
			len += prev_ep->nlen;
			break;
		case '\\':
			if (p[1] != '\0')
				++p;
			break;
		}

	gp = sp->gp;
	if (len != wordlen) {
		/*
		 * Copy argument into temporary space, replacing file
		 * names.  Allocate temporary space if necessary.
		 */
		if (F_ISSET(gp, G_TMP_INUSE)) {
			if ((tpath = malloc(len)) == NULL) {
				msgq(sp, M_ERROR,
				    "Error: %s.", strerror(errno));
				return (1);
			}
		} else {
			BINC(sp, gp->tmp_bp, gp->tmp_blen, len);
			tpath = gp->tmp_bp;
			F_SET(gp, G_TMP_INUSE);
		}

		for (p = tpath; *word; ++word)
			switch(*word) {
			case '%':
				memmove(p, ep->name, ep->nlen);
				p += ep->nlen;
				break;
			case '#':
				memmove(p, prev_ep->name, prev_ep->nlen);
				p += prev_ep->nlen;
				break;
			case '\\':
				if (p[1] != '\0')
					++p;
				/* FALLTHROUGH */
			default:
				*p++ = *word;
			}
		p = tpath;
	} else
		p = word;

	glob(p,
	    GLOB_APPEND | GLOB_NOSORT | GLOB_NOCHECK | GLOB_QUOTE | GLOB_TILDE,
	    NULL, globp);

	if (tpath == gp->tmp_bp)
		F_CLR(gp, G_TMP_INUSE);
	else
		free(tpath);
	return (0);
}
