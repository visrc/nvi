/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_util.c,v 5.9 1993/05/20 20:31:54 bostic Exp $ (Berkeley) $Date: 1993/05/20 20:31:54 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

/*
 * ex_getline --
 *	Return a line from the terminal.
 */
int
ex_getline(sp, fp, lenp)
	SCR *sp;
	FILE *fp;
	size_t *lenp;
{
	size_t off;
	int ch;
	char *p;

	for (off = 0, p = sp->ibp;; ++off) {
		ch = getc(fp);
		if (off >= sp->ibp_len) {
			BINC(sp, sp->ibp, sp->ibp_len, off + 1);
			p = sp->ibp + off;
		}
		if (ch == EOF || ch == '\n') {
			if (ch == EOF && !off)
				return (1);
			*lenp = off;
			return (0);
		}
		*p++ = ch;
	}
	/* NOTREACHED */
}
