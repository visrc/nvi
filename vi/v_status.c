/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.1 1992/04/18 19:39:46 bostic Exp $ (Berkeley) $Date: 1992/04/18 19:39:46 $";
#endif /* not lint */

#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"
#include "extern.h"

/*
 * v_status --
 *	Run the ex "file" command to show the file's status.
 */
MARK
v_status()
{
	CMDARG cmd;

	SETCMDARG(cmd, C_FILE, 2, cursor, cursor, 0, "");
	ex_file(&cmd);
	return cursor;
}
