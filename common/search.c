/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 5.2 1992/05/07 12:45:44 bostic Exp $ (Berkeley) $Date: 1992/05/07 12:45:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <db.h>
#include <stdio.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "regexp.h"
#include "extern.h"

static regexp *re;		/* Compiled version of the pattern. */
enum direction searchdir;

#define	START(dir) { \
	if (set) \
		searchdir = dir; \
	if (ptrn && *ptrn) { \
		l = parseptrn(ptrn); \
		if (*l) { \
			delta = *l ? strtol(l, &ep, 10) : 0; \
			if (eptrn) \
				*eptrn = ep; \
		} else \
			if (eptrn) \
				*eptrn = l; \
		if (re) \
			free(re); \
		if ((re = regcomp(ptrn)) == NULL) \
			return (NULL); \
	} else if (re == NULL) { \
		msg("No previous regular expression."); \
		return (NULL); \
	} \
}

MARK *
f_search(fm, ptrn, eptrn, set)
	MARK *fm;
	char *ptrn, **eptrn;
	int set;
{
	static MARK rval;
	u_long coff, lno;
	long delta;
	int wrapped;
	char *ep, *l;

	START(FORWARD);

	wrapped = 0;
	for (lno = fm->lno, coff = fm->cno;; ++lno, coff = 1)
		if ((l = file_line(curf, lno, NULL)) == NULL) {
			if (wrapped) {
				msg("Pattern not found.");
				return (NULL);
			}
			if (!ISSET(O_WRAPSCAN)) {
		msg("Reached end-of-file without finding the pattern.");
				return (NULL);
			}
			lno = 1;
			wrapped = 1;
		} else if (regexec(re, l + coff, coff == 1)) {
			if (wrapped && ISSET(O_WARN))
				msg("Search wrapped.");
			if (delta) {
				if (delta < 0 && (u_long)delta > lno) {
					msg("Search offset before line 1.");
					return (NULL);
				}
				lno += delta;
				if (file_line(curf, lno, NULL) == NULL) {
					msg("Search offset past end-of-file.");
					return (NULL);
				}
				rval.lno = lno;
				rval.cno = 1;
			} else {
				rval.lno = lno;
				rval.cno = re->startp[0] - l;
			}
			return (&rval);
		}
	/* NOTREACHED */
}

MARK *
b_search(fm, ptrn, eptrn, set)
	MARK *fm;
	char *ptrn, **eptrn;
	int set;
{
	static MARK rval;
	u_long coff, lno;
	long delta;
	int last, try, wrapped;
	char *ep, *l;

	START(BACKWARD);

	wrapped = 0;
	for (lno = fm->lno, coff = fm->cno;; --lno, coff = 1) {
		if (lno == 0) {
			if (!ISSET(O_WRAPSCAN)) {
		msg("Reached top-of-file without finding the pattern.");
				return (NULL);
			}
			if ((lno = file_lline(curf)) == 0)
				return (NULL);
			wrapped = 1;
			continue;
		} else if (lno == fm->lno && wrapped) {
			msg("Pattern not found.");
			return (NULL);
		}

		if ((l = file_line(curf, lno, NULL)) == NULL)
			return (NULL);

		if (regexec(re, l, 1) && (int)(re->startp[0] - l) < coff) {
			if (wrapped && ISSET(O_WARN))
				msg("Search wrapped.");
			if (delta) {
				if (delta < 0 && (u_long)delta > lno) {
					msg("Search offset before line 1.");
					return (NULL);
				}
				lno += delta;
				if (file_line(curf, lno, NULL) == NULL) {
					msg("Search offset past end-of-file.");
					return (NULL);
				}
				rval.lno = lno;
				rval.cno = 1;
			} else {
				/* Find the last acceptable one in this line. */
				for (;;) {
					last = (int)(re->startp[0] - l);
					try = (int)(re->endp[0] - l);
					if (try < 0 ||
					    !regexec(re, &l[try], 0) ||
					    (int)(re->startp[0] - l) < coff)
						break;
				}
				rval.lno = lno;
				rval.cno = last;
			}
			return (&rval);
		}
	}
	/* NOTREACHED */
}
