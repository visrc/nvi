/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: util.c,v 8.77 1994/09/18 11:57:39 bostic Exp $ (Berkeley) $Date: 1994/09/18 11:57:39 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"

/*
 * binc --
 *	Increase the size of a buffer.
 */
void *
binc(sp, bp, bsizep, min)
	SCR *sp;			/* sp MAY BE NULL!!! */
	void *bp;
	size_t *bsizep, min;
{
	size_t csize;

	/* If already larger than the minimum, just return. */
	if (min && *bsizep >= min)
		return (bp);

	csize = *bsizep + MAX(min, 256);
	REALLOC(sp, bp, void *, csize);

	if (bp == NULL) {
		/*
		 * Theoretically, realloc is supposed to leave any already
		 * held memory alone if it can't get more.  Don't trust it.
		 */
		*bsizep = 0;
		return (NULL);
	}
	/*
	 * Memory is guaranteed to be zero-filled, various parts of
	 * nvi depend on this.
	 */
	memset((char *)bp + *bsizep, 0, csize - *bsizep);
	*bsizep = csize;
	return (bp);
}

/*
 * nonblank --
 *	Set the column number of the first non-blank character
 *	including or after the starting column.  On error, set
 *	the column to 0, it's safest.
 */
int
nonblank(sp, ep, lno, cnop)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t *cnop;
{
	char *p;
	size_t cnt, len, off;

	/* Default. */
	off = *cnop;
	*cnop = 0;

	/* Get the line. */
	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			return (0);
		GETLINE_ERR(sp, lno);
		return (1);
	}

	/* Set the offset. */
	if (len == 0 || off >= len)
		return (0);

	for (cnt = off, p = &p[off],
	    len -= off; len && isblank(*p); ++cnt, ++p, --len);

	/* Set the return. */
	*cnop = len ? cnt : cnt - 1;
	return (0);
}

/*
 * tail --
 *	Return tail of a path.
 */
char *
tail(path)
	char *path;
{
	char *p;

	if ((p = strrchr(path, '/')) == NULL)
		return (path);
	return (p + 1);
}

/*
 * set_alt_name --
 *	Set the alternate file name.
 *
 * Set the alternate file name.  It's a routine because I wanted some place
 * to hang this comment.  The alternate file name (normally referenced using
 * the special character '#' during file expansion and in the vi ^^ command)
 * is set by almost all ex commands that take file names as arguments.  The
 * rules go something like this:
 *
 *    1: If any ex command takes a file name as an argument (except for the
 *	 :next command), the alternate file name is set to that file name.
 *	 This excludes the command ":e<CR>" and ":w !command" as no file name
 *       was specified.  Note, historically, the :source command did not set
 *	 the alternate file name.  It does in nvi, for consistency.
 *
 *    2: However, if any ex command sets the current file name, e.g. the
 *	 ":e file" or ":rew" commands succeed, then the alternate file name
 *	 is set to the previous file's current file name, if it had one.
 *	 This includes the :file command and excludes the ":e<CR>" command.
 *
 *    3: However, if it's a read or write command with a file argument and
 *	 the current file name had not yet been set, the file name becomes
 *	 the current file name, and the alternate file name is unchanged.
 *
 * If the user edits a temporary file, there may be times when there is no
 * alternative file name.  A name argument of NULL turns it off.
 */
void
set_alt_name(sp, name)
	SCR *sp;
	char *name;
{
	if (sp->alt_name != NULL)
		free(sp->alt_name);
	if (name == NULL)
		sp->alt_name = NULL;
	else if ((sp->alt_name = strdup(name)) == NULL)
		msgq(sp, M_SYSERR, NULL);
}

/*
 * v_strdup --
 *	Strdup for wide character strings with an associated length.
 */
CHAR_T *
v_strdup(sp, str, len)
	SCR *sp;
	const CHAR_T *str;
	size_t len;
{
	CHAR_T *copy;

	MALLOC(sp, copy, CHAR_T *, len + 1);
	if (copy == NULL)
		return (NULL);
	memmove(copy, str, len * sizeof(CHAR_T));
	copy[len] = '\0';
	return (copy);
}

/*
 * vi_putchar --
 *	Functional version of putchar, for tputs.
 */
void
vi_putchar(ch)
	int ch;
{
	(void)putchar(ch);
}

#ifdef DEBUG
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
TRACE(SCR *sp, const char *fmt, ...)
#else
TRACE(sp, fmt, va_alist)
	SCR *sp;
	char *fmt;
	va_dcl
#endif
{
	FILE *tfp;
	va_list ap;

	if ((tfp = sp->gp->tracefp) == NULL)
		return;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(tfp, fmt, ap);
	va_end(ap);

	(void)fflush(tfp);
}
#endif
