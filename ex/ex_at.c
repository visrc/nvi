/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.8 1992/04/18 18:33:20 bostic Exp $ (Berkeley) $Date: 1992/04/18 18:33:20 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_at(cmdp)
	CMDARG *cmdp;
{
	static int lastbuf, recurse;
	static char rstack[UCHAR_MAX];
	size_t len;
	int name, rval;
	u_char *buf;

	if ((name = cmdp->buffer) == '\0' && (name = lastbuf) == '\0') {
		msg("No previous buffer to execute.");
		return (1);
	}
		
	if (recurse == 0)
		bzero(rstack, sizeof(rstack));
	else if (rstack[name]) {
		msg("Buffer %c already occurs in this command.", name);
		return (1);
	}

	if ((buf = cb2str(name, &len)) == NULL)
		rval = 1;
	else {
		rstack[name] = 1;
		++recurse;
		rval = ex_cstring((char *)buf, len, 1);
		--recurse;
		free(buf);
	} 
	return (rval);
}
