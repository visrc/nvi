/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: util.c,v 8.68 1994/07/05 14:53:03 bostic Exp $ (Berkeley) $Date: 1994/07/05 14:53:03 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
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
int
binc(sp, argp, bsizep, min)
	SCR *sp;			/* sp MAY BE NULL!!! */
	void *argp;
	size_t *bsizep, min;
{
	size_t csize;
	void *bpp;

	/* If already larger than the minimum, just return. */
	if (min && *bsizep >= min)
		return (0);

	bpp = *(char **)argp;
	csize = *bsizep + MAX(min, 256);
	REALLOC(sp, bpp, void *, csize);

	if (bpp == NULL) {
		/*
		 * Theoretically, realloc is supposed to leave any already
		 * held memory alone if it can't get more.  Don't trust it.
		 */
		*bsizep = 0;
		return (1);
	}
	/*
	 * Memory is guaranteed to be zero-filled, various parts of
	 * nvi depend on this.
	 */
	memset((char *)bpp + *bsizep, 0, csize - *bsizep);
	*(char **)argp = bpp;
	*bsizep = csize;
	return (0);
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
 * set_window_size --
 *	Set the window size, the row may be provided as an argument.
 */
int
set_window_size(sp, set_row, sigwinch)
	SCR *sp;
	u_int set_row;
	int sigwinch;
{
	struct winsize win;
	size_t col, row;
	int rval, user_set;
	ARGS *argv[2], a, b;
	char *s, buf[2048];

	/*
	 * Get the screen rows and columns.  If the values are wrong, it's
	 * not a big deal -- as soon as the user sets them explicitly the
	 * environment will be set and the screen package will use the new
	 * values.
	 *
	 * Try TIOCGWINSZ.
	 */
	row = col = 0;
#ifdef TIOCGWINSZ
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) != -1) {
		row = win.ws_row;
		col = win.ws_col;
	}
#endif
	/* If here because of a signal, TIOCGWINSZ is all we trust. */
	if (sigwinch) {
		if (row == 0 || col == 0) {
			msgq(sp, M_SYSERR, "TIOCGWINSZ");
			return (1);
		}
		goto sigw;
	}

	/*
	 * If TIOCGWINSZ failed, or had entries of 0, try termcap.  Get
	 * the terminal from the environment, the options code hasn't been
	 * initialized yet.  We permit the TERM environmental variable to
	 * be uninitialized, we may be running ex.
	 */
	if (row == 0 || col == 0) {
		if ((s = getenv("TERM")) == NULL)
			goto noterm;
#ifdef SYSV_CURSES
		if (row == 0)
			if ((rval = tigetnum("lines")) < 0)
				msgq(sp, M_SYSERR, "tigetnum: lines");
			else
				row = rval;
		if (col == 0)
			if ((rval = tigetnum("cols")) < 0)
				msgq(sp, M_SYSERR, "tigetnum: cols");
			else
				col = rval;
#else
		if (term_tgetent(sp, buf, s))
			goto noterm;
		if (row == 0)
			if ((rval = tgetnum("li")) < 0)
				msgq(sp, M_ERR,
				    "no \"li\" capability for %s", s);
			else
				row = rval;
		if (col == 0)
			if ((rval = tgetnum("co")) < 0)
				msgq(sp, M_ERR,
				    "no \"co\" capability for %s", s);
			else
				col = rval;
#endif
	}

	/*
	 * If it's something completely unreasonable, stop now.  The actual
	 * error (probably ENOMEM) is likely to be much less informative.
	 */
	if (row > 1500) {
		msgq(sp, M_ERR, "%lu rows isn't believable", (u_long)row);
		return (1);
	}
	if (col > 1500) {
		msgq(sp, M_ERR, "%lu columns isn't believable", (u_long)col);
		return (1);
	}

	/* If nothing else, well, it's probably a VT100. */
noterm:	if (row == 0)
		row = 24;
	if (col == 0)
		col = 80;

	/* POSIX 1003.2 requires the environment to override. */
	if ((s = getenv("LINES")) != NULL)
		row = strtol(s, NULL, 10);
	if ((s = getenv("COLUMNS")) != NULL)
		col = strtol(s, NULL, 10);

	/* But, if we got an argument for the rows, use it. */
	if (set_row)
		row = set_row;

sigw:	a.bp = buf;
	b.bp = NULL;
	b.len = 0;
	argv[0] = &a;
	argv[1] = &b;;

	/*
	 * Tell the options code that the screen size has changed.
	 * Since the user didn't do the set, clear the set bits.
	 */
	user_set = F_ISSET(&sp->opts[O_LINES], OPT_SET);
	a.len = snprintf(buf, sizeof(buf), "lines=%u", row);
	if (opts_set(sp, argv))
		return (1);
	if (user_set)
		F_CLR(&sp->opts[O_LINES], OPT_SET);
	user_set = F_ISSET(&sp->opts[O_COLUMNS], OPT_SET);
	a.len = snprintf(buf, sizeof(buf), "columns=%u", col);
	if (opts_set(sp, argv))
		return (1);
	if (user_set)
		F_CLR(&sp->opts[O_COLUMNS], OPT_SET);
	return (0);
}

/*
 * set_alt_name --
 *	Set the alternate file name.
 *
 * Swap the alternate file name.  It's a routine because I wanted some place
 * to hang this comment.  The alternate file name (normally referenced using
 * the special character '#' during file expansion) is set by many
 * operations.  In the historic vi, the commands "ex", and "edit" obviously
 * set the alternate file name because they switched the underlying file.
 * Less obviously, the "read", "file", "write" and "wq" commands set it as
 * well.  In this implementation, some new commands have been added to the
 * list.  Where it gets interesting is that the alternate file name is set
 * multiple times by some commands.  If an edit attempt fails (for whatever
 * reason, like the current file is modified but as yet unwritten), it is
 * set to the file name that the user was unable to edit.  If the edit
 * succeeds, it is set to the last file name that was edited.  Good fun.
 *
 * If the user edits a temporary file, there are time when there isn't an
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
 * baud_from_bval --
 *	Return the baud rate using the standard defines.
 */
u_long
baud_from_bval(sp)
	SCR *sp;
{
	if (!F_ISSET(sp->gp, G_TERMIOS_SET))
		return (9600);

	/*
	 * XXX
	 * There's no portable way to get a "baud rate" -- cfgetospeed(3)
	 * returns the value associated with some #define, which we may
	 * never have heard of, or which may be a purely local speed.  Vi
	 * only cares if it's SLOW (w300), slow (w1200) or fast (w9600).
	 * Try and detect the slow ones, and default to fast.
	 */
	switch (cfgetospeed(&sp->gp->original_termios)) {
	case B50:
	case B75:
	case B110:
	case B134:
	case B150:
	case B200:
	case B300:
	case B600:
		return (600);
	case B1200:
		return (1200);
	}
	return (9600);
}

/*
 * v_strdup --
 *	Strdup for wide character strings with an associated length.
 */
CHAR_T *
v_strdup(sp, str, len)
	SCR *sp;
	CHAR_T *str;
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
