/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.3 1992/04/05 09:24:46 bostic Exp $ (Berkeley) $Date: 1992/04/05 09:24:46 $";
#endif /* not lint */

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "excmd.h"
#include "extern.h"

int
ex_at(cmdp)
	CMDARG *cmdp;
{
	static int lastbuf, recurse;
	static char rstack[UCHAR_MAX];
	int buf, len, rval;
	char copy[1024];

	if ((buf = cmdp->buffer) == '\0' && (buf = lastbuf) == '\0') {
		msg("No previous buffer to execute.");
		return (1);
	}
		
	if (recurse++) {
		if (rstack[buf]) {
			msg("Buffer %c already occurs in this command.");
			return (1);
		}
		rstack[buf] = 1;
	} else
		bzero(rstack, UCHAR_MAX);
			
	len = cb2str(buf, copy, sizeof(copy));
	if (len <= 0) {
		msg("Buffer %c is empty.", buf);
		return (1);
	}
	if (len >= sizeof buf) {
		msg("Buffer %c is too large to execute.", buf);
		return (1);
	}
	rval = exstring(copy, len);
	--recurse;
	return (rval);
}
