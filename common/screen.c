/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: screen.c,v 5.1 1993/03/28 19:08:33 bostic Exp $ (Berkeley) $Date: 1993/03/28 19:08:33 $";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"

/*
 * scr_def --
 *	Do the default initialization of an SCR structure.
 */
int
scr_def(orig, sp)
	SCR *orig, *sp;
{
	extern u_char asciilen[];		/* XXX */
	extern char *asciiname[];		/* XXX */

	memset(sp, 0, sizeof(SCR));		/* Zero out the structure. */

	if (orig != NULL) {			/* Copy some stuff. */
		*sp = *orig;

#ifdef notdef
		tag_copy(orig, sp);
		seq_copy(orig, sp);
		opt_copy(orig, sp);
#endif
		HDR_APPEND(sp, orig, next, prev, SCR);
	} else {
		sp->searchdir = NOTSET;
		sp->csearchdir = CNOTSET;
		sp->inc_lastch = '+';
		sp->inc_lastval = 1;
	}

	sp->clen = asciilen;			/* XXX */
	sp->cname = asciiname;			/* XXX */
	F_SET(sp, S_REDRAW);

	return (0);
}
