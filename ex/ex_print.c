/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_print.c,v 10.6 1995/09/21 10:57:49 bostic Exp $ (Berkeley) $Date: 1995/09/21 10:57:49 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "../vi/vi.h"

static int ex_prchars __P((SCR *, const char *, size_t *, size_t, u_int, int));

/*
 * ex_list -- :[line [,line]] l[ist] [count] [flags]
 *
 *	Display the addressed lines such that the output is unambiguous.
 *
 * PUBLIC: int ex_list __P((SCR *, EXCMD *));
 */
int
ex_list(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	if (ex_print(sp, cmdp,
	    &cmdp->addr1, &cmdp->addr2, cmdp->iflags | E_C_LIST))
		return (1);
	sp->lno = cmdp->addr2.lno;
	sp->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_number -- :[line [,line]] nu[mber] [count] [flags]
 *
 *	Display the addressed lines with a leading line number.
 *
 * PUBLIC: int ex_number __P((SCR *, EXCMD *));
 */
int
ex_number(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	if (ex_print(sp, cmdp,
	    &cmdp->addr1, &cmdp->addr2, cmdp->iflags | E_C_HASH))
		return (1);
	sp->lno = cmdp->addr2.lno;
	sp->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_pr -- :[line [,line]] p[rint] [count] [flags]
 *
 *	Display the addressed lines.
 *
 * PUBLIC: int ex_pr __P((SCR *, EXCMD *));
 */
int
ex_pr(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	if (ex_print(sp, cmdp, &cmdp->addr1, &cmdp->addr2, cmdp->flags))
		return (1);
	sp->lno = cmdp->addr2.lno;
	sp->cno = cmdp->addr2.cno;
	return (0);
}

/*
 * ex_print --
 *	Print the selected lines.
 *
 * PUBLIC: int ex_print __P((SCR *, EXCMD *, MARK *, MARK *, u_int32_t));
 */
int
ex_print(sp, cmdp, fp, tp, flags)
	SCR *sp;
	EXCMD *cmdp;
	MARK *fp, *tp;
	u_int32_t flags;
{
	GS *gp;
	const char *p;
	recno_t from, to;
	size_t col, len;
	char buf[10];

	NEEDFILE(sp, cmdp);

	gp = sp->gp;
	for (from = fp->lno, to = tp->lno; from <= to; ++from) {
		col = 0;

		/*
		 * Display the line number.  The %6 format is specified
		 * by POSIX 1003.2, and is almost certainly large enough.
		 * Check, though, just in case.
		 */
		if (LF_ISSET(E_C_HASH)) {
			if (from <= 999999) {
				snprintf(buf, sizeof(buf), "%6ld  ", from);
				p = buf;
			} else
				p = "TOOBIG  ";
			if (ex_prchars(sp, p, &col, 8, 0, 0))
				return (1);
		}

		/*
		 * Display the line.  The format for E_C_PRINT isn't very good,
		 * especially in handling end-of-line tabs, but they're almost
		 * backward compatible.
		 */
		if ((p = file_gline(sp, from, &len)) == NULL) {
			FILE_LERR(sp, from);
			return (1);
		}

		if (len == 0 && !LF_ISSET(E_C_LIST))
			(void)ex_puts(sp, "\n");
		else if (ex_ldisplay(sp, p, len, col, flags))
			return (1);

		if (INTERRUPTED(sp))
			break;
	}
	return (0);
}

/*
 * ex_ldisplay --
 *	Display a line without any preceding number.
 *
 * PUBLIC: int ex_ldisplay __P((SCR *, const char *, size_t, size_t, u_int));
 */
int
ex_ldisplay(sp, p, len, col, flags)
	SCR *sp;
	const char *p;
	size_t len, col;
	u_int flags;
{
	if (len > 0 && ex_prchars(sp, p, &col, len, LF_ISSET(E_C_LIST), 0))
		return (1);
	if (!INTERRUPTED(sp) && LF_ISSET(E_C_LIST)) {
		p = "$";
		if (ex_prchars(sp, p, &col, 1, LF_ISSET(E_C_LIST), 0))
			return (1);
	}
	if (!INTERRUPTED(sp))
		(void)ex_puts(sp, "\n");
	return (0);
}

/*
 * ex_scprint --
 *	Display a line for the substitute with confirmation routine.
 *
 * PUBLIC: int ex_scprint __P((SCR *, MARK *, MARK *));
 */
int
ex_scprint(sp, fp, tp)
	SCR *sp;
	MARK *fp, *tp;
{
	const char *p;
	size_t col, len;

	col = 0;
	if (O_ISSET(sp, O_NUMBER)) {
		p = "        ";
		if (ex_prchars(sp, p, &col, 8, 0, 0))
			return (1);
	}

	if ((p = file_gline(sp, fp->lno, &len)) == NULL) {
		FILE_LERR(sp, fp->lno);
		return (1);
	}

	if (ex_prchars(sp, p, &col, fp->cno, 0, ' '))
		return (1);
	if (!INTERRUPTED(sp) &&
	    ex_prchars(sp, p, &col, tp->cno - fp->cno, 0, '^'))
		return (1);
	if (!INTERRUPTED(sp)) {
		p = "[ynq]";		/* XXX: should be msg_cat. */
		if (ex_prchars(sp, p, &col, 5, 0, 0))
			return (1);
	}
	(void)ex_fflush(sp);
	return (0);
}

/*
 * ex_prchars --
 *	Local routine to dump characters to the screen.
 */
static int
ex_prchars(sp, p, colp, len, flags, repeatc)
	SCR *sp;
	const char *p;
	size_t *colp, len;
	u_int flags;
	int repeatc;
{
	CHAR_T ch, *kp;
	GS *gp;
	size_t col, tlen, ts;

	if (O_ISSET(sp, O_LIST))
		LF_SET(E_C_LIST);
	gp = sp->gp;
	ts = O_VAL(sp, O_TABSTOP);
	for (col = *colp; len--;)
		if ((ch = *p++) == '\t' && !LF_ISSET(E_C_LIST))
			for (tlen = ts - col % ts;
			    col < sp->cols && tlen--; ++col) {
				(void)ex_printf(sp,
				    "%c", repeatc ? repeatc : ' ');
				if (INTERRUPTED(sp))
					goto intr;
			}
		else {
			kp = KEY_NAME(sp, ch);
			tlen = KEY_LEN(sp, ch);
			if (!repeatc  && col + tlen < sp->cols) {
				(void)ex_puts(sp, kp);
				col += tlen;
			} else
				for (; tlen--; ++kp, ++col) {
					if (col == sp->cols) {
						col = 0;
						(void)ex_puts(sp, "\n");
					}
					(void)ex_printf(sp,
					    "%c", repeatc ? repeatc : *kp);
					if (INTERRUPTED(sp))
						goto intr;
				}
		}
intr:	*colp = col;
	return (0);
}

/*
 * Buffers for the ex data.  The screen support doesn't do any character
 * buffering of any kind.  We do it here so that we're not calling the
 * screen output routines on every character.
 *
 * XXX
 * Move into screen specific ex private area, change to grow dynamically.
 */
static size_t off;
static char buf[1024];

/*
 * ex_printf --
 *	Ex's version of printf.
 *
 * PUBLIC: int ex_printf __P((SCR *, const char *, ...));
 */
int
#ifdef __STDC__
ex_printf(SCR *sp, const char *fmt, ...)
#else
ex_printf(sp, fmt, va_alist)
	SCR *sp;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	size_t n;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	/*
	 * XXX
	 * We discard any characters past the first sizeof(buf) / 2.
	 */
	n = vsnprintf(buf + off, sizeof(buf) / 2, fmt, ap);
	va_end(ap);

	off += n;
	if (off > sizeof(buf) / 2) {
		(void)sp->gp->scr_msg(sp, M_NONE, buf, off);
		off = 0;
	}
	return (n);
}

/*
 * ex_puts --
 *	Ex's version of puts.
 *
 * PUBLIC: int ex_puts __P((SCR *, const char *));
 */
int
ex_puts(sp, str)
	SCR *sp;
	const char *str;
{
	int n;

	for (n = 0; *str != '\0'; ++n) {
		if (off > sizeof(buf)) {
			(void)sp->gp->scr_msg(sp, M_NONE, buf, off);
			off = 0;
		}
		buf[off++] = *str++;
	}
	return (n);
}

/*
 * ex_fflush --
 *	Ex's version of fflush.
 *
 * PUBLIC: int ex_fflush __P((SCR *sp));
 */
int
ex_fflush(sp)
	SCR *sp;
{
	if (off != 0) {
		(void)sp->gp->scr_msg(sp, M_NONE, buf, off);
		off = 0;
	}
	(void)sp->gp->scr_msg(sp, M_NONE, NULL, 0);
	return (0);
}
