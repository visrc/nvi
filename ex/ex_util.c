/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_util.c,v 5.3 1993/02/28 14:00:47 bostic Exp $ (Berkeley) $Date: 1993/02/28 14:00:47 $";
#endif /* not lint */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>

#include "vi.h"
#include "options.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

u_char *
ex_getline(ep, fp, lenp)
	EXF *ep;
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
			if (binc(ep, &bp, &bplen, off)) {
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

/*
 * ex_msg --
 *	Display a message.
 */
void
#ifdef __STDC__
ex_msg(EXF *ep, u_int flags, const char *fmt, ...)
#else
ex_msg(ep, flags, fmt, va_alist)
	EXF *ep;
	u_int flags;
        char *fmt;
        va_dcl
#endif
{
        va_list ap;
#ifdef __STDC__
        va_start(ap, fmt);
#else
        va_start(ap);
#endif

	/* If extra information message, check user's wishes. */
	if (!(flags & (M_DISPLAY | M_ERROR)) && !ISSET(O_VERBOSE))
		return;

	(void)vfprintf(ep->stdfp, fmt, ap);
	(void)putc('\n', ep->stdfp);
}
