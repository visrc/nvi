/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: util.c,v 8.22 1993/11/18 10:55:08 bostic Exp $ (Berkeley) $Date: 1993/11/18 10:55:08 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
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
	 * It's possible to enter msg when there's no screen to hold
	 * the message.  Always check sp before using it, and, if it's
	 * NULL, use __global_list.
	 */
	switch (mt) {
	case M_BERR:
		if (sp != NULL && !O_ISSET(sp, O_VERBOSE)) {
			F_SET(sp, S_BELLSCHED);
			return;
		}
		mt = M_ERR;
		break;
	case M_VINFO:
		if (sp != NULL && !O_ISSET(sp, O_VERBOSE))
			return;
		mt = M_INFO;
		break;
	case M_ERR:
	case M_INFO:
	case M_SYSERR:
		break;
	default:
		abort();
	}

	/* Length is the min length of the message or the buffer. */
	if (mt == M_SYSERR)
		len = snprintf(msgbuf, sizeof(msgbuf), "Error: %s%s%s.",
		    fmt == NULL ? "" : fmt, fmt == NULL ? "" : ": ",
		    strerror(errno));
	else
		len = vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	if (len > sizeof(msgbuf))
		len = sizeof(msgbuf);

	msg_app(__global_list, sp, mt == M_ERR ? 1 : 0, msgbuf, len);
}

/*
 * msg_app --
 *	Append a message into the queue.  This can fail, but there's
 *	nothing we can do if it does.
 */
void
msg_app(gp, sp, inv_video, p, len)
	GS *gp;
	SCR *sp;
	int inv_video;
	char *p;
	size_t len;
{
	static int reenter;		/* STATIC: Re-entrancy check. */
	MSG *mp, *nmp;

	/*
	 * It's possible to reenter msg when it allocates space.
	 * We're probably dead anyway, but no reason to drop core.
	 */
	if (reenter)
		return;
	reenter = 1;

	/*
	 * Find an empty structure, or allocate a new one.  Use the
	 * screen structure if possible, otherwise the global one.
	 */
	if (sp != NULL) {
		if ((mp = sp->msgq.lh_first) == NULL) {
			if ((mp = malloc(sizeof(MSG))) == NULL)
				goto ret;
			memset(mp, 0, sizeof(MSG));
			LIST_INSERT_HEAD(&sp->msgq, mp, q);
			goto store;
		}
	} else if ((mp = gp->msgq.lh_first) == NULL) {
		if ((mp = malloc(sizeof(MSG))) == NULL)
			goto ret;
		memset(mp, 0, sizeof(MSG));
		LIST_INSERT_HEAD(&gp->msgq, mp, q);
		goto store;
	}
	while (!F_ISSET(mp, M_EMPTY) && mp->q.le_next != NULL)
		mp = mp->q.le_next;
	if (!F_ISSET(mp, M_EMPTY)) {
		if ((nmp = malloc(sizeof(MSG))) == NULL)
			goto ret;
		memset(nmp, 0, sizeof(MSG));
		LIST_INSERT_AFTER(mp, nmp, q);
		mp = nmp;
	}

	/* Get enough memory for the message. */
store:	if (len > mp->blen && binc(sp, &mp->mbuf, &mp->blen, len))
		goto ret;

	/* Store the message. */
	memmove(mp->mbuf, p, len);
	mp->len = len;
	mp->flags = inv_video ? M_INV_VIDEO : 0;

ret:	reenter = 0;
}

/*
 * msgrpt --
 *	Report on the lines that changed.
 *
 * !!!
 * Historic vi documentation (USD:15-8) claimed that "The editor will also
 * always tell you when a change you make affects text which you cannot see."
 * This isn't true -- edit a large file and do "100d|1".  We don't implement
 * this semantic as it would require that we track each line that changes
 * during a command instead of just keeping count.
 */
int
msg_rpt(sp, is_message)
	SCR *sp;
	int is_message;
{
	static const char *const action[] = {
		"added", "changed", "copied", "deleted", "joined", "moved",	
		"put", "left shifted", "right shifted", "yanked", NULL,
	};
	recno_t total;
	u_long rval;
	int first, cnt;
	size_t blen, len;
	const char *const *ap;
	char *bp, *p, number[40];

	if ((rval = O_VAL(sp, O_REPORT)) == 0)
		goto norpt;

	GET_SPACE(sp, bp, blen, 512);
	p = bp;

	total = 0;
	for (ap = action, cnt = 0, first = 1; *ap != NULL; ++ap, ++cnt)
		if (sp->rptlines[cnt] != 0) {
			total += sp->rptlines[cnt];
			len = snprintf(number, sizeof(number),
			    "%s%lu line%s %s", first ? "" : "; ",
			    sp->rptlines[cnt],
			    sp->rptlines[cnt] > 1 ? "s" : "", *ap);
			memmove(p, number, len);
			p += len;
			first = 0;
		}

	/*
	 * If nothing to report, return.  Note that the number of lines
	 * must be > than the user's value, not >=.  This is historic
	 * practice and means that users cannot report on single line
	 * changes.
	 */
	if (total > rval) {
		*p = '\0';

		if (is_message)
			msgq(sp, M_INFO, "%s", bp);
		else
			ex_printf(EXCOOKIE, "%s\n", bp);
	}

	FREE_SPACE(sp, bp, blen);

	/* Clear after each report. */
norpt:	memset(sp->rptlines, 0, sizeof(sp->rptlines));
	return (0);
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
		msgq(sp, M_SYSERR, NULL);
		*bsizep = 0;
		return (1);
	}
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
set_window_size(sp, set_row, ign_env)
	SCR *sp;
	u_int set_row;
	int ign_env;
{
	struct winsize win;
	size_t col, row;
	int user_set;
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

	/*
	 * POSIX 1003.2 requires the environment to override, however,
	 * if we're here because of a signal, we don't want to use the
	 * old values.
	 */
	if (!ign_env) {
		if ((s = getenv("ROWS")) != NULL)
			row = strtol(s, NULL, 10);
		if ((s = getenv("COLUMNS")) != NULL)
			col = strtol(s, NULL, 10);
	}

	/* But, if we got an argument for the rows, use it. */
	if (set_row)
		row = set_row;

	argv[0] = buf;
	argv[1] = NULL;

	/*
	 * Tell the options code that the screen size has changed.
	 * Since the user didn't do the set, clear the set bits.
	 */
	user_set = F_ISSET(&sp->opts[O_LINES], OPT_SET);
	(void)snprintf(buf, sizeof(buf), "ls=%u", row);
	if (opts_set(sp, argv))
		return (1);
	if (user_set)
		F_CLR(&sp->opts[O_LINES], OPT_SET);
	user_set = F_ISSET(&sp->opts[O_COLUMNS], OPT_SET);
	(void)snprintf(buf, sizeof(buf), "co=%u", col);
	if (opts_set(sp, argv))
		return (1);
	if (user_set)
		F_CLR(&sp->opts[O_COLUMNS], OPT_SET);
	return (0);
}

/*
 * set_alt_fname --
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
 */
void
set_alt_fname(sp, fname)
	SCR *sp;
	char *fname;
{
	if (sp->alt_fname != NULL)
		FREE(sp->alt_fname, strlen(sp->alt_fname));
	if ((sp->alt_fname = strdup(fname)) == NULL)
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
	speed_t v;

	switch (v = cfgetospeed(&sp->gp->original_termios)) {
	case B50:
		return (50);
	case B75:
		return (75);
	case B110:
		return (110);
	case B134:
		return (134);
	case B150:
		return (150);
	case B200:
		return (200);
	case B300:
		return (300);
	case B600:
		return (600);
	case B1200:
		return (1200);
	case B1800:
		return (1800);
	case B2400:
		return (2400);
	case B4800:
		return (4800);
	case B0:				/* Hangup -- ignore. */
	case B9600:
		return (9600);
	case B19200:
		return (19200);
	case B38400:
		return (38400);
	default:
		/*
		 * EXTA and EXTB aren't required by POSIX 1003.1, and
		 * are almost certainly the same as some of the above
		 * values, so they can't be part of the case statement.
		 */
#ifdef EXTA
		if (v == EXTA)
			return (19200);
#endif
#ifdef EXTB
		if (v == EXTB)
			return (38400);
#endif
		msgq(sp, M_ERR, "Unknown terminal baud rate %u.\n", v);
		return (9600);
	}
}
