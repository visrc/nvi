/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_at.c,v 5.6 1992/04/15 09:12:52 bostic Exp $ (Berkeley) $Date: 1992/04/15 09:12:52 $";
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
	int buf, csize, len, rval;
	char *cp, copy[1024];

	if ((buf = cmdp->buffer) == '\0' && (buf = lastbuf) == '\0') {
		msg("No previous buffer to execute.");
		return (1);
	}
		
	if (recurse++) {
		if (rstack[buf]) {
			msg("Buffer %c already occurs in this command.", buf);
			return (1);
		}
		rstack[buf] = 1;
	} else
		bzero(rstack, UCHAR_MAX);
			
	for (cp = copy, csize = sizeof(copy);;) {
		len = cb2str(buf, cp, csize);
		if (len <= 0) {
			msg("Buffer %c is empty.", buf);
			return (1);
		}
		if (len <= csize)
			break;
		if ((cp == malloc(csize = len)) == NULL) {
			msg("Buffer %c is too large to execute.", buf);
			return (1);
		}
	}
	if (cp != copy)
		free(cp);
	rval = ex_cstring(cp, len);
	--recurse;
	return (rval);
}
