/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 8.3 1993/08/25 16:48:04 bostic Exp $ (Berkeley) $Date: 1993/08/25 16:48:04 $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

int
v_at(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	EXCMDARG cmd;

        SETCMDARG(cmd, C_AT, 0, OOBLNO, OOBLNO, 0, NULL);
	cmd.buffer = vp->buffer;
        return (sp->s_ex_cmd(sp, ep, &cmd, rp));
}
