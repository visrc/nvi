/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: util.c,v 5.38 1993/05/07 09:09:25 bostic Exp $ (Berkeley) $Date: 1993/05/07 09:09:25 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "vi.h"

/*
 * msgq --
 *	Display a message.
 */
void
#ifdef __STDC__
msgq(SCR *sp, enum msgtype mt, const char *fmt, ...)
#else
msgq(sp, mt, fmt, va_alist)
	SCR *sp;
	enum msgtype mt;
        char *fmt;
        va_dcl
#endif
{
        va_list ap;
	int len;
	char msgbuf[1024];

#ifdef __STDC__
        va_start(ap, fmt);
#else
        va_start(ap);
#endif
	/*
	 * It's possible to reenter msg when it allocates space.  We're
	 * probably dead anyway, but no reason to drop core.
	 */
	if (F_ISSET(sp, S_MSGREENTER))
		return;
	F_SET(sp, S_MSGREENTER);

	switch (mt) {
	case M_BERR:
		if (!O_ISSET(sp, O_VERBOSE)) {
			F_SET(sp, S_BELLSCHED);
			F_CLR(sp, S_MSGREENTER);
			return;
		}
		mt = M_ERR;
		break;
	case M_ERR:
		break;
	case M_INFO:
		break;
	case M_VINFO:
		if (!O_ISSET(sp, O_VERBOSE)) {
			F_CLR(sp, S_MSGREENTER);
			return;
		}
		mt = M_INFO;
		break;
	default:
		abort();
	}

	/* Length is the min length of the message or the buffer. */
	len = vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	if (len > sizeof(msgbuf))
		len = sizeof(msgbuf);

	msga(NULL, sp, mt == M_ERR ? 1 : 0, msgbuf, len);

	F_CLR(sp, S_MSGREENTER);
}

/*
 * msga --
 *	Append a message into the queue.  This can fail, but there's
 *	absolutely nothing we can do if it does.
 */
void
msga(gp, sp, inv_video, p, len)
	GS *gp;
	SCR *sp;
	int inv_video;
	char *p;
	size_t len;
{
	MSG *mp;
	int new;

	/*
	 * Find an empty structure, or allocate a new one.  Use the
	 * screen structure if possible, otherwise the global one.
	 */
	new = 0;
	if (sp != NULL)
		if (sp->msgp == NULL) {
			if ((sp->msgp = malloc(sizeof(MSG))) == NULL)
				return;
			new = 1;
			mp = sp->msgp;
		} else {
			mp = sp->msgp;
			goto loop;
		}
	else if (gp->msgp == NULL) {
		if ((gp->msgp = malloc(sizeof(MSG))) == NULL)
			return;
		mp = gp->msgp;
		new = 1;
	} else {
		mp = gp->msgp;
loop:		for (;
		    !F_ISSET(mp, M_EMPTY) && mp->next != NULL; mp = mp->next);
		if (!F_ISSET(mp, M_EMPTY)) {
			if ((mp->next = malloc(sizeof(MSG))) == NULL)
				return;
			mp = mp->next;
			new = 1;
		}
	}

	/* Initialize new structures. */
	if (new)
		memset(mp, 0, sizeof(MSG));

	/* Store the message. */
	if (len > mp->blen && binc(sp, &mp->mbuf, &mp->blen, len))
		return;

	memmove(mp->mbuf, p, len);
	mp->len = len;
	mp->flags = inv_video ? M_INV_VIDEO : 0;
}

/*
 * binc --
 *	Increase the size of a buffer.
 */
int
binc(sp, argp, bsizep, min)
	SCR *sp;			/* MAY BE NULL */
	void *argp;
	size_t *bsizep, min;
{
	void *bpp;
	size_t csize;

	/* If already larger than the minimum, just return. */
	csize = *bsizep;
	if (min && csize >= min)
		return (0);

	csize += MAX(min, 256);
	bpp = *(char **)argp;

	/* For non-ANSI C realloc implementations. */
	if (bpp == NULL)
		bpp = malloc(csize);
	else
		bpp = realloc(bpp, csize);
	if (bpp == NULL) {
		if (sp != NULL)
			msgq(sp, M_ERR, "Error: %s.", strerror(errno));
		*bsizep = 0;
		return (1);
	}
	*(char **)argp = bpp;
	*bsizep = csize;
	return (0);
}

/*
 * nonblank --
 *	Return the column number of the first non-blank character of the
 *	line.
 */
int
nonblank(sp, ep, lno, cnop)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t *cnop;
{
	char *p;
	size_t cnt, len;

	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		if (file_lline(sp, ep) == 0) {
			*cnop = 0;
			return (0);
		}
		GETLINE_ERR(sp, lno);
		return (1);
	}
	if (len == 0) {
		*cnop = 0;
		return (0);
	}
	for (cnt = 0; len && isspace(*p); ++cnt, ++p, --len);
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
 * onhup --
 *	Handle SIGHUP, restoring sanity to the terminal, preserving the file.
 */
/* ARGSUSED */
void
onhup(signo)
	int signo;
{
#ifdef XXX_RIP_THIS_OUT
	/* Restore the terminal's sanity. */
	endwin();

	/* If we had a temp file going, then preserve it. */
	if (tmpnum > 0 && tmpfd >= 0) {
		(void)close(tmpfd);
		(void)snprintf(buf, sizeof(buf),
		    "%s %s", _PATH_PRESERVE, tmpname);
		(void)esystem(PVAL(O_SHELL), tmpblk.c);
	}

	/* Delete any old temp files. */
	cutend();
#endif

	/* Exit with the proper exit status. */
	(void)signal(SIGHUP, SIG_DFL);
	(void)kill(0, SIGHUP);

	/* NOTREACHED */
	exit (1);
}

/*
 * set_window_size --
 *	Set the window size, the row may be provided as an argument.
 */
int
set_window_size(sp, set_row)
	SCR *sp;
	u_int set_row;
{
	struct winsize win;
	size_t col, row;
	int isset;
	char *argv[2], *s, buf[2048];

	row = 24;
	col = 80;

	/*
	 * Get the screen rows and columns.  If the values are wrong, it's
	 * not a big deal -- as soon as the user sets them explicitly the
	 * environment will be set and the screen package will use the new
	 * values.
	 *
	 * Try TIOCGWINSZ, followed by the termcap entry.
	 */
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		row = win.ws_row;
		col = win.ws_col;
	} else {
		s = NULL;
		if (F_ISSET(&sp->opts[O_TERM], OPT_SET))
			s = O_STR(sp, O_TERM);
		else
			s = getenv("TERM");
		if (s != NULL && tgetent(buf, s) == 1) {
			row = tgetnum("li");
			col = tgetnum("co");
		}
	}

	/* POSIX 1003.2 requires the environment to override. */
	if ((s = getenv("ROWS")) != NULL)
		row = strtol(s, NULL, 10);
	if ((s = getenv("COLUMNS")) != NULL)
		col = strtol(s, NULL, 10);

	/* But, if we got an argument for the rows, use it. */
	if (set_row)
		row = set_row;

	argv[0] = buf;
	argv[1] = NULL;

	/*
	 * Tell the options code that the screen size has changed.
	 * Since the user didn't do the set, clear the set bits.
	 */
	isset = F_ISSET(&sp->opts[O_LINES], OPT_SET);
	(void)snprintf(buf, sizeof(buf), "ls=%u", row ? row : win.ws_row);
	if (opts_set(sp, argv))
		return (1);
	if (isset)
		F_CLR(&sp->opts[O_LINES], OPT_SET);
	isset = F_ISSET(&sp->opts[O_COLUMNS], OPT_SET);
	(void)snprintf(buf, sizeof(buf), "co=%u", win.ws_col);
	if (opts_set(sp, argv))
		return (1);
	if (isset)
		F_CLR(&sp->opts[O_COLUMNS], OPT_SET);
	return (0);
}
