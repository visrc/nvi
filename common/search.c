/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: search.c,v 5.3 1992/05/15 11:01:20 bostic Exp $ (Berkeley) $Date: 1992/05/15 11:01:20 $";
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

static regexp *sre;			/* Saved RE. */
enum direction searchdir;		/* Search direction. */

#define	START(dir) {							\
	delta = 0;							\
	if (ptrn) {							\
		l = parseptrn(ptrn);					\
		if (*l) {						\
			delta = strtol(l, &ep, 10);			\
			if (eptrn)					\
				*eptrn = ep;				\
		} else							\
			if (eptrn)					\
				*eptrn = l;				\
		if ((re = regcomp(ptrn)) == NULL)			\
			return (NULL);					\
		if (set) {						\
			searchdir = dir;				\
			if (sre)					\
				free(sre);				\
			sre = re;					\
		}							\
	} else if (sre == NULL) {					\
		msg("No previous regular expression.");			\
		return (NULL);						\
	} else								\
		re = sre;						\
}

MARK *
f_search(fm, ptrn, eptrn, set)
	MARK *fm;
	char *ptrn, **eptrn;
	int set;
{
	static MARK rval;
	MARK *rvp;
	regexp *re;
	u_long coff, lno;
	long delta;
	int sol, wrapped;
	char *ep, *l;

	START(FORWARD);

	sol = wrapped = 0;
	for (lno = fm->lno, coff = fm->cno + 1;; ++lno, sol = 1)
		if ((l = file_gline(curf, lno, NULL)) == NULL) {
			if (wrapped) {
				msg("Pattern not found.");
				rvp = NULL;
				break;
			}
			if (!ISSET(O_WRAPSCAN)) {
		msg("Reached end-of-file without finding the pattern.");
				rvp = NULL;
				break;
			}
			lno = 1;
			wrapped = 1;
		} else if (regexec(re, l + coff, sol)) {
			if (wrapped && ISSET(O_WARN))
				msg("Search wrapped.");
			if (delta) {
				if (delta < 0 && (u_long)delta > lno) {
					msg("Search offset before line 1.");
					return (NULL);
				}
				lno += delta;
				if (file_gline(curf, lno, NULL) == NULL) {
					msg("Search offset past end-of-file.");
					return (NULL);
				}
				rval.lno = lno;
				rval.cno = 0;
			} else {
				rval.lno = lno;
				rval.cno = re->startp[0] - l;
			}
			rvp = &rval;
			break;
		}
	if (!set)
		free(re);
	return (rvp);
}

MARK *
b_search(fm, ptrn, eptrn, set)
	MARK *fm;
	char *ptrn, **eptrn;
	int set;
{
	static MARK rval;
	MARK *rvp;
	regexp *re;
	u_long coff, lno;
	long delta;
	int last, try, wrapped;
	char *ep, *l;

	START(BACKWARD);

	/* If in the first column, start searching on the last line. */
	if (fm->cno == 0) {
		fm->cno = 0;
		lno = fm->lno - 1;
	} else
		lno = fm->lno;

	wrapped = 0;
	for (coff = fm->cno;; --lno, coff = 0) {
		if (lno == 0) {
			if (!ISSET(O_WRAPSCAN)) {
		msg("Reached top-of-file without finding the pattern.");
				rvp = NULL;
				break;
			}
			if ((lno = file_lline(curf)) == 0) {
				rvp = NULL;
				break;
			}
			wrapped = 1;
			continue;
		} else if (lno == fm->lno && wrapped) {
			msg("Pattern not found.");
			rvp = NULL;
			break;
		}

		if ((l = file_gline(curf, lno, NULL)) == NULL) {
			rvp = NULL;
			break;
		}

		if (regexec(re, l, 1) && (int)(re->startp[0] - l) < coff) {
			if (wrapped && ISSET(O_WARN))
				msg("Search wrapped.");
			if (delta) {
				if (delta < 0 && (u_long)delta > lno) {
					msg("Search offset before line 1.");
					rvp = NULL;
					break;
				}
				lno += delta;
				if (file_gline(curf, lno, NULL) == NULL) {
					msg("Search offset past end-of-file.");
					rvp = NULL;
					break;
				}
				rval.lno = lno;
				rval.cno = 0;
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
			rvp = &rval;
			break;
		}
	}
	if (!set)
		free(re);
	return (rvp);
}
