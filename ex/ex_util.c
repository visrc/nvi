/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_util.c,v 5.1 1993/02/11 19:42:06 bostic Exp $ (Berkeley) $Date: 1993/02/11 19:42:06 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"

u_char *
ex_getline(fp, lenp)
	FILE *fp;
	size_t *lenp;
{
	static size_t bplen;
	static u_char *bp;
	size_t off;
	int ch;
	u_char *p;

	for (off = 0, p = bp;; ++off) {
		ch = getc(fp);
		if (off >= bplen) {
			if (binc(&bp, &bplen, off)) {
				*lenp = bplen = 0;
				bp = NULL;
				return (NULL);
			}
			p = bp + off;
		}
		if (ch == EOF || ch == '\n') {
			if (ch == EOF && !off)
				return (NULL);
			*lenp = off;
			return (bp);
		}
		*p++ = ch;
	}
	/* NOTREACHED */
}
