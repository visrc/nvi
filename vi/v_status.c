/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_status.c,v 5.3 1992/04/22 08:10:32 bostic Exp $ (Berkeley) $Date: 1992/04/22 08:10:32 $";
#endif /* not lint */

#include <sys/types.h>
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
	EXCMDARG cmd;

	SETCMDARG(cmd, C_FILE, 2, cursor, cursor, 0, "");
	ex_file(&cmd);
	return cursor;
}
