/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: getc.c,v 5.9 1993/02/16 20:08:08 bostic Exp $ (Berkeley) $Date: 1993/02/16 20:08:08 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "vcmd.h"
#include "search.h"
#include "getc.h"

static MARK m;
static size_t len;
static u_char *p;

/*
 * getc_init --
 *	Initialize getc routines.
 */
int
getc_init(ep, fm, chp)
	EXF *ep;
	MARK *fm;
	int *chp;
{
	u_char *p;

	m = *fm;
	if ((p = file_gline(ep, fm->lno, &len)) == NULL) {
		if (file_lline(ep) == 0)
			v_eol(ep, NULL);
		else
			GETLINE_ERR(ep, fm->lno);
		return (1);
	}
	*chp = len == 0 ? EMPTYLINE : p[m.cno];
	return (0);
}

/*
 * getc_next --
 *	Retrieve the next character.
 */
int
getc_next(ep, dir, chp)
	EXF *ep;
	enum direction dir;
	int *chp;
{
	MARK save;

	save = m;
	if (dir == FORWARD)
		if (len == 0 || m.cno == len - 1) {
			m.cno = 0;		/* EOF; restore the cursor. */
			if ((p = file_gline(ep, ++m.lno, &len)) == NULL) {
				m = save;
				return (0);
			}
			if (len == 0) {
				*chp = EMPTYLINE;
				return (1);
			}
		} else
			++m.cno;
	else /* if (dir == BACKWARD) */
		if (m.cno == 0) {		/* EOF; restore the cursor. */
			if ((p = file_gline(ep, --m.lno, &len)) == NULL) {
				m = save;
				return (0);
			}
			if (len == 0) {
				*chp = EMPTYLINE;
				return (1);
			}
			m.cno = len - 1;
		} else
			--m.cno;
	*chp = p[m.cno];
	return (1);
}

/*
 * getc_set --
 *	Return the last cursor position.
 */
void
getc_set(ep, rp)
	EXF *ep;
	MARK *rp;
{
	*rp = m;
}
