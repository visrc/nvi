/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_paragraph.c,v 5.15 1993/05/08 12:37:54 bostic Exp $ (Berkeley) $Date: 1993/05/08 12:37:54 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * Paragraphs are empty lines after text or values from the paragraph or
 * section options.
 */
static char *makelist __P((SCR *));

/*
 * v_paragraphf -- [count]}
 *	Move forward count paragraphs.
 */
int
v_paragraphf(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	enum { P_INTEXT, P_INBLANK } pstate;
	size_t len;
	recno_t cnt, lno;
	char *p, *list, *lp;

	/* Get macro list. */
	if ((list = makelist(sp)) == NULL)
		return (1);

	/* Figure out what state we're currently in. */
	lno = fm->lno;
	if ((p = file_gline(sp, ep, lno, &len)) == NULL)
		goto eof;

	/*
	 * If we start in text, we want to switch states 2 * N - 1
	 * times, in non-text, 2 * N times.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	cnt *= 2;
	if (len == 0)
		pstate = P_INBLANK;
	else {
		--cnt;
		pstate = P_INTEXT;
	}

	for (;;) {
		if ((p = file_gline(sp, ep, ++lno, &len)) == NULL)
			goto eof;
		switch (pstate) {
		case P_INTEXT:
			if (p[0] == '.' && len >= 2)
				for (lp = list; *lp; lp += 2)
					if (lp[0] == p[1] &&
					    (lp[1] == ' ' || lp[1] == p[2]) &&
					    !--cnt)
						goto found;
			if (len == 0) {
				if (!--cnt)
					goto found;
				pstate = P_INBLANK;
			}
			break;
		case P_INBLANK:
			if (len != 0) {
				if (!--cnt) {
found:					rp->lno = lno;
					rp->cno = 0;
					free(list);
					return (0);
				}
				pstate = P_INTEXT;
			}
			break;
		default:
			abort();
		}
	}
eof:	free(list);

	/*
	 * EOF is a movement sink, however, the } command historically
	 * moved to the end of the last line if repeatedly invoked.
	 */
	if (fm->lno != lno - 1) {
		rp->lno = lno - 1;
		rp->cno = len ? len - 1 : 0;
		return (0);
	}
	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL)
		GETLINE_ERR(sp, fm->lno);
	if (fm->cno != (len ? len - 1 : 0)) {
		rp->lno = lno - 1;
		rp->cno = len ? len - 1 : 0;
		return (0);
	}
	v_eof(sp, ep, NULL);
	return (1);
}

/*
 * v_paragraphb -- [count]{
 *	Move forward count paragraph.
 */
int
v_paragraphb(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	enum { P_INTEXT, P_INBLANK } pstate;
	size_t len;
	recno_t cnt, lno;
	char *p, *list, *lp;

	/* Get macro list. */
	if ((list = makelist(sp)) == NULL)
		return (1);

	/*
	 * The { command historically moved to the beginning of the first
	 * line if invoked on the first line.
	 *
	 * Check for SOF.
	 */
	if (fm->lno <= 1) {
		if (fm->cno == 0) {
			v_sof(sp, NULL);
			return (1);
		}
		rp->lno = 1;
		rp->cno = 0;
		return (0);
	}

	/* Figure out what state we're currently in. */
	lno = fm->lno;
	if ((p = file_gline(sp, ep, lno, &len)) == NULL)
		goto sof;

	/*
	 * If we start in text, we want to switch states 2 * N - 1
	 * times, in non-text, 2 * N times.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	cnt *= 2;
	if (len == 0)
		pstate = P_INBLANK;
	else {
		--cnt;
		pstate = P_INTEXT;
	}

	for (;;) {
		if ((p = file_gline(sp, ep, --lno, &len)) == NULL)
			goto sof;
		switch (pstate) {
		case P_INTEXT:
			if (p[0] == '.' && len >= 2)
				for (lp = list; *lp; lp += 2)
					if (lp[0] == p[1] &&
					    (lp[1] == ' ' || lp[1] == p[2]) &&
					    !--cnt)
						goto found;
			if (len == 0) {
				if (!--cnt)
					goto found;
				pstate = P_INBLANK;
			}
			break;
		case P_INBLANK:
			if (len != 0) {
				if (!--cnt) {
found:					rp->lno = lno;
					rp->cno = 0;
					free(list);
					return (0);
				}
				pstate = P_INTEXT;
			}
			break;
		default:
			abort();
		}
	}
sof:	free(list);

	/* SOF is a movement sink. */
	rp->lno = 1;
	rp->cno = 0;
	return (0);
}

static char *
makelist(sp)
	SCR *sp;
{
	size_t s1, s2;
	char *list;

	/* Search for either a paragraph or section option macro. */
	s1 = strlen(O_STR(sp, O_PARAGRAPHS));
	if (s1 & 1) {
		msgq(sp, M_ERR,
		    "Paragraph options must be in groups of two characters.");
		return (NULL);
	}
	s2 = strlen(O_STR(sp, O_SECTIONS));
	if (s2 & 1) {
		msgq(sp, M_ERR,
		    "Section options must be in groups of two characters.");
		return (NULL);
	}
	if ((list = malloc(s1 + s2 + 1)) == NULL) {
		msgq(sp, M_ERR, "%s", strerror(errno));
		return (NULL);
	}
	memmove(list, O_STR(sp, O_PARAGRAPHS), s1);
	memmove(list + s1, O_STR(sp, O_SECTIONS), s2 + 1);
	return (list);
}
