/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_at.c,v 8.5 1994/03/08 19:41:10 bostic Exp $ (Berkeley) $Date: 1994/03/08 19:41:10 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

/*
 * v_at -- @
 *	Execute a buffer.
 */
int
v_at(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	EXCMDARG cmd;

        SETCMDARG(cmd, C_AT, 0, OOBLNO, OOBLNO, 0, NULL);
	cmd.buffer = vp->buffer;
        return (sp->s_ex_cmd(sp, ep, &cmd, &vp->m_final));
}
