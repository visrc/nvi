/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_section.c,v 5.1 1992/06/16 07:25:50 bostic Exp $ (Berkeley) $Date: 1992/06/16 07:25:50 $";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "options.h"
#include "extern.h"

/*
 * v_sectionf -- [count]]]
 *	Move forward count sections.
 */
int
v_sectionf(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	char *p, *list, *lp;

	/* Get macro list. */
	if ((list = PVAL(O_SECTIONS)) == NULL)
		return (1);
	if (strlen(list) & 1) {
		msg("Section options must be in groups of two characters.");
		return (1);
	}

	rp->cno = 0;
	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (lno = fm->lno; p = file_gline(curf, ++lno, &len);) {
		if (len == 0 || p[0] != '.' || len < 2)
			continue;
		/* Check for macro. */
		for (lp = list; *lp; lp += 2)
			if (lp[0] == p[1] &&
			    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
found:				rp->lno = lno;
				return (0);
			}
	}

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
 * v_sectionb -- [count][[
 *	Move forward count sections.
 */
int
v_sectionb(vp, fm, tm, rp)
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t cnt, lno;
	char *p, *list, *lp;

	/* Check for SOF. */
	if (fm->lno <= 1) {
		v_sof(NULL);
		return (1);
	}

	if ((list = PVAL(O_SECTIONS)) == NULL)
		return (1);
	if (strlen(list) & 1) {
		msg("Section options must be in groups of two characters.");
		return (1);
	}

	rp->cno = 0;
	cnt = vp->flags & VC_C1SET ? vp->count : 1;
	for (lno = fm->lno; p = file_gline(curf, --lno, &len);) {
		if (len == 0 || p[0] != '.' || len < 2)
			continue;
		/* Check for macro. */
		for (lp = list; *lp; lp += 2)
			if (lp[0] == p[1] &&
			    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
found:				rp->lno = lno;
				return (0);
			}
	}

	/* SOF is a movement sink. */
sof:	rp->lno = 1;
	return (0);
}
