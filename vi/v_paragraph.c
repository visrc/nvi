/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_paragraph.c,v 5.2 1992/10/10 14:01:47 bostic Exp $ (Berkeley) $Date: 1992/10/10 14:01:47 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "extern.h"

/*
 * Paragraphs are empty lines after text or values from the paragraph or
 * section options.  The historic version of vi did some fairly strange
 * stuff when moving backwards through the file by paragraphs.  This code
 * duplicates its behavior.
 */

static u_char *makelist __P((void));

/*
 * v_paragraphf -- [count]}
 *	Move forward count paragraphs.
 */
int
v_paragraphf(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	u_char *p, *list, *lp;

	/* Get macro list. */
	if ((list = makelist()) == NULL)
		return (1);

	rp->cno = 0;

	/* If at an empty line, skip to text. */
	for (lno = fm->lno + 1;; ++lno) {
		if ((p = file_gline(curf, lno, &len)) == NULL)
			goto eof;
		if (len)
			break;
	}

	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (; p = file_gline(curf, lno, &len); ++lno) {
		if (len == 0) {
			if (!--cnt)
				goto found;
			/* Skip to text. */
			for (;;) {
				if ((p = file_gline(curf, lno++, &len)) == NULL)
					goto eof;
				if (len)
					break;
			}
		}
		if (p[0] != '.' || len < 2)
			continue;
		/* Check for macro. */
		for (lp = list; *lp; lp += 2)
			if (lp[0] == p[1] &&
			    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
found:				rp->lno = lno;
				free(list);
				return (0);
			}
	}
	free(list);

	/* EOF is a movement sink. */
eof:	if (fm->lno != lno - 1) {
		rp->lno = lno - 1;
		rp->cno = len ? len - 1 : 0;
		return (0);
	}
	v_eof(NULL);
	return (1);
}

/*
 * v_paragraphb -- [count]{
 *	Move forward count paragraph.
 */
int
v_paragraphb(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	u_char *p, *list, *lp;

	/* Get macro list. */
	if ((list = makelist()) == NULL)
		return (1);

	/* Check for SOF. */
	if (fm->lno <= 1) {
		v_sof(NULL);
		return (1);
	}

	rp->cno = 0;

	/* If at an empty line, skip to text. */
	for (lno = fm->lno - 1;; --lno) {
		if ((p = file_gline(curf, lno, &len)) == NULL)
			goto sof;
		if (len)
			break;
	}

	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (; p = file_gline(curf, lno, &len); --lno) {
		if (len == 0) {
			if (!--cnt)
				goto found;
			/* Skip to text. */
			for (;;) {
				if ((p = file_gline(curf, lno--, &len)) == NULL)
					goto sof;
				if (len)
					break;
			}
		}
		if (p[0] != '.' || len < 2)
			continue;
		/* Check for macro. */
		for (lp = list; *lp; lp += 2)
			if (lp[0] == p[1] &&
			    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
found:				rp->lno = lno;
				free(list);
				return (0);
			}
	}
	free(list);

	/* SOF is a movement sink. */
sof:	rp->lno = 1;
	return (0);
}

static u_char *
makelist()
{
	size_t s1, s2;
	u_char *list;

	/* Search for either a paragraph or section option macro. */
	s1 = USTRLEN(PVAL(O_PARAGRAPHS));
	if (s1 & 1) {
		msg("Paragraph options must be in groups of two characters.");
		return (NULL);
	}
	s2 = USTRLEN(PVAL(O_SECTIONS));
	if (s2 & 1) {
		msg("Section options must be in groups of two characters.");
		return (NULL);
	}
	if ((list = malloc(s1 + s2 + 1)) == NULL) {
		msg("%s", strerror(errno));
		return (NULL);
	}
	bcopy(PVAL(O_PARAGRAPHS), list, s1);
	bcopy(PVAL(O_SECTIONS), list + s1, s2 + 1);
	return (list);
}
