/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_util.c,v 8.3 1993/11/13 18:02:32 bostic Exp $ (Berkeley) $Date: 1993/11/13 18:02:32 $";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

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
	EX_PRIVATE *exp;
	size_t off;
	int ch;
	char *p;

	exp = EXP(sp);
	for (off = 0, p = exp->ibp;; ++off) {
		ch = getc(fp);
		if (off >= exp->ibp_len) {
			BINC(sp, exp->ibp, exp->ibp_len, off + 1);
			p = exp->ibp + off;
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
