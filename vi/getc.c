/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: getc.c,v 5.15 1993/05/08 09:54:25 bostic Exp $ (Berkeley) $Date: 1993/05/08 09:54:25 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

#define	GB	ep->getc_bp
#define	GL	ep->getc_blen
#define	GM	ep->getc_m

/*
 * getc_init --
 *	Initialize getc routines.
 */
int
getc_init(sp, ep, fm, chp)
	SCR *sp;
	EXF *ep;
	MARK *fm;
	int *chp;
{
	GM = *fm;
	if ((GB = file_gline(sp, ep, fm->lno, &GL)) == NULL) {
		if (file_lline(sp, ep) == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, fm->lno);
		return (1);
	}
	*chp = GL == 0 ? EMPTYLINE : GB[GM.cno];
	return (0);
}

/*
 * getc_next --
 *	Retrieve the next character.
 */
int
getc_next(sp, ep, dir, chp)
	SCR *sp;
	EXF *ep;
	enum direction dir;
	int *chp;
{
	MARK save;

	save = GM;
	switch (dir) {
	case FORWARD:
		if (GL == 0 || GM.cno == GL - 1) {
			GM.cno = 0;		/* EOF; restore the cursor. */
			if ((GB = file_gline(sp, ep, ++GM.lno, &GL)) == NULL) {
				GM = save;
				return (0);
			}
			if (GL == 0) {
				*chp = EMPTYLINE;
				return (1);
			}
		} else
			++GM.cno;
		break;
	case BACKWARD:
		if (GM.cno == 0) {		/* EOF; restore the cursor. */
			if ((GB = file_gline(sp, ep, --GM.lno, &GL)) == NULL) {
				GM = save;
				return (0);
			}
			if (GL == 0) {
				*chp = EMPTYLINE;
				return (1);
			}
			GM.cno = GL - 1;
		} else
			--GM.cno;
		break;
	default:
		abort();
	}
	*chp = GB[GM.cno];
	return (1);
}

/*
 * getc_set --
 *	Return the last cursor position.
 */
void
getc_set(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	*rp = GM;
}
