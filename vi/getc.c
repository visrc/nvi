/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: getc.c,v 5.1 1992/06/08 09:27:35 bostic Exp $ (Berkeley) $Date: 1992/06/08 09:27:35 $";
#endif /* not lint */

#include <sys/types.h>
#include <stddef.h>

#include "vi.h"
#include "exf.h"
#include "mark.h"
#include "getc.h"
#include "extern.h"

static MARK m;
static size_t len;
static char *p;

/*
 * getc_init --
 *	Initialize getc routines.
 */
int
getc_init(fm, chp)
	MARK *fm;
	int *chp;
{
	m = *fm;
	EGETLINE(p, fm->lno, len);
	*chp = len == 0 ? EMPTYLINE : p[m.cno];
	return (0);
}

/*
 * getc_next --
 *	Retrieve the next character.
 */
int
getc_next(dir, chp)
	enum direction dir;
	int *chp;
{
	MARK save;

	save = m;
	if (dir == FORWARD)
		if (len == 0 || m.cno == len - 1) {
			m.cno = 0;
			GETLINE(p, ++m.lno, len);
			if (p == NULL) {	/* EOF; restore the cursor. */
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
		if (m.cno == 0) {
			GETLINE(p, --m.lno, len);
			if (p == NULL) {	/* EOF; restore the cursor. */
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
getc_set(rp)
	MARK *rp;
{
	*rp = m;
}
