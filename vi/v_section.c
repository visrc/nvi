/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_section.c,v 5.10 1993/02/28 14:01:58 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:01:58 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "options.h"
#include "vcmd.h"

/*
 * In historic vi, the section commands ignored empty lines, unlike the
 * paragraph commands, which was probably okay, but also moved to the
 * start of the last line when there where no more sections instead of
 * the end of the last line.  This has been changed to be more like the
 * paragraphs command.
 *
 * In historic vi, a "function" was defined as the first character on
 * the line being an open brace.
 */

/*
 * v_sectionf -- [count]]]
 *	Move forward count sections/functions.
 */
int
v_sectionf(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	u_char *p, *list, *lp;

	/* Get macro list. */
	if ((list = PVAL(O_SECTIONS)) == NULL)
		return (1);
	if (USTRLEN(list) & 1) {
		ep->msg(ep, M_ERROR,
		    "Section options must be in groups of two characters.");
		return (1);
	}

	rp->cno = 0;
	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (lno = fm->lno; p = file_gline(ep, ++lno, &len);)
		switch(len) {
		case 0:
			break;;
		case 1:
			if (p[0] == '{' && !--cnt) {
				rp->lno = lno;
				return (0);
			}
			break;
		default:
			if (p[0] != '.')
				break;
			/* Check for macro. */
			for (lp = list; *lp; lp += 2)
				if (lp[0] == p[1] &&
				    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
					rp->lno = lno;
					return (0);
				}
			break;
		}

	/* EOF is a movement sink. */
	if (fm->lno != lno - 1) {
		rp->lno = lno - 1;
		rp->cno = len ? len - 1 : 0;
		return (0);
	}
	v_eof(ep, NULL);
	return (1);
}

/*
 * v_sectionb -- [count][[
 *	Move backward count sections/functions.
 */
int
v_sectionb(ep, vp, fm, tm, rp)
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	u_char *p, *list, *lp;

	/* Check for SOF. */
	if (fm->lno <= 1) {
		v_sof(ep, NULL);
		return (1);
	}

	if ((list = PVAL(O_SECTIONS)) == NULL)
		return (1);
	if (USTRLEN(list) & 1) {
		ep->msg(ep, M_ERROR,
		    "Section options must be in groups of two characters.");
		return (1);
	}

	rp->cno = 0;
	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (lno = fm->lno; p = file_gline(ep, --lno, &len);)
		switch(len) {
		case 0:
			break;
		case 1:
			if (p[0] == '{' && !--cnt) {
				rp->lno = lno;
				return (0);
			}
			break;
		default:
			if (p[0] != '.')
				break;
			/* Check for macro. */
			for (lp = list; *lp; lp += 2)
				if (lp[0] == p[1] &&
				    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
					rp->lno = lno;
					return (0);
				}
			break;
		}

	/* SOF is a movement sink. */
	rp->lno = 1;
	return (0);
}
