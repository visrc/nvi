/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_section.c,v 8.3 1993/08/31 17:15:26 bostic Exp $ (Berkeley) $Date: 1993/08/31 17:15:26 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * In historic vi, the section commands ignored empty lines, unlike the
 * paragraph commands, which was probably okay.  However, they also moved
 * to the start of the last line when there where no more sections instead
 * of the end of the last line like the paragraph commands.  I've changed
 * the latter behaviore to match the paragraphs command.
 *
 * In historic vi, a "function" was defined as the first character of the
 * line being an open brace, which could be followed by anything.  This
 * implementation follows that historic practice.
 */

/* Macro to do a check on each line. */
#define	CHECK {								\
	if (len == 0)							\
		continue;						\
	if (p[0] == '{') {						\
		if (!--cnt) {						\
			rp->cno = 0;					\
			rp->lno = lno;					\
			return (0);					\
		}							\
		continue;						\
	}								\
	if (p[0] != '.' || len < 3)					\
		continue;						\
	for (lp = list; *lp; lp += 2)					\
		if (lp[0] == p[1] &&					\
		    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {	\
			rp->cno = 0;					\
			rp->lno = lno;					\
			return (0);					\
		}							\
}

/*
 * v_sectionf -- [count]]]
 *	Move forward count sections/functions.
 */
int
v_sectionf(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	char *p, *list, *lp;

	/* Get macro list. */
	if ((list = O_STR(sp, O_SECTIONS)) == NULL)
		return (1);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	for (lno = fm->lno; (p = file_gline(sp, ep, ++lno, &len)) != NULL;)
		CHECK;

	/* EOF is a movement sink. */
	if (fm->lno != lno - 1) {
		rp->lno = lno - 1;
		rp->cno = len ? len - 1 : 0;
		return (0);
	}
	v_eof(sp, ep, NULL);
	return (1);
}

/*
 * v_sectionb -- [count][[
 *	Move backward count sections/functions.
 */
int
v_sectionb(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	char *p, *list, *lp;

	/* Check for SOF. */
	if (fm->lno <= 1) {
		v_sof(sp, NULL);
		return (1);
	}

	/* Get macro list. */
	if ((list = O_STR(sp, O_SECTIONS)) == NULL)
		return (1);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	for (lno = fm->lno; (p = file_gline(sp, ep, --lno, &len)) != NULL;)
		CHECK;

	/* SOF is a movement sink. */
	rp->lno = 1;
	return (0);
}
