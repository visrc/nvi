/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.48 1993/02/28 17:47:58 bostic Exp $ (Berkeley) $Date: 1993/02/28 17:47:58 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "seq.h"
#include "screen.h"
#include "term.h"

u_char special[UCHAR_MAX];		/* Special characters. */

u_char *gb_cb;				/* Return buffer. */
u_char *gb_qb;				/* Quote buffer. */
u_char *gb_wb;				/* Widths buffer. */
u_long gb_blen;				/* Buffer lengths. */

static void	check_sigwinch __P((EXF *));
static int	ttyread __P((EXF *, u_char *, int, int));

/*
 * gb_init --
 *	Initialize the special array.  Basically, this array has a value for
 *	each special character that we can use in a switch statement.  This
 *	speeds up lookup and normal insertion tremendously.
 */
void
gb_init(ep)
	EXF *ep;
{
	struct termios t;

	memset(special, 0, sizeof(special));

	if (tcgetattr(STDIN_FILENO, &t))
		return;

	/* Keys that are treated specially. */
	special['^'] = K_CARAT;
	special['\004'] = K_CNTRLD;
	special['\022'] = K_CNTRLR;
	special['\032'] = K_CNTRLZ;
	special['\r'] = K_CR;
	special['\033'] = K_ESCAPE;
	special['\f'] = K_FORMFEED;
	special['\n'] = K_NL;
	special['\t'] = K_TAB;
	special['\t'] = K_TAB;
	special[t.c_cc[VERASE]] = K_VERASE;
	special[t.c_cc[VKILL]] = K_VKILL;
	special[t.c_cc[VLNEXT]] = K_VLNEXT;
	special[t.c_cc[VWERASE]] = K_VWERASE;
	special['0'] = K_ZERO;

	/* Start off with some memory. */
	(void)gb_inc(ep);
}

/*
 * gb_inc
 *	Increase the size of the gb buffers.
 */
int
gb_inc(ep)
	EXF *ep;
{
	gb_blen += 256;
	if ((gb_cb = realloc(gb_cb, gb_blen)) == NULL ||
	    (gb_qb = realloc(gb_qb, gb_blen)) == NULL ||
	    (gb_wb = realloc(gb_wb, gb_blen)) == NULL) {
			ep->msg(ep, M_ERROR,
			    "Input too long: %s.", strerror(errno));
			if (gb_cb)
				free(gb_cb);
			if (gb_qb)
				free(gb_qb);
			if (gb_wb)
				free(gb_wb);
			gb_cb = gb_qb = gb_wb = NULL;
			gb_blen = 0;
			return (1);
		}
	return (0);
}

/*
 * getkey --
 *	This function reads in a keystroke, as well as handling mapped keys
 *	and executed cut buffers.
 */
int
getkey(ep, flags)
	EXF *ep;
	u_int flags;			/* GB_MAPCOMMAND, GB_MAPINPUT */
{
	static int nkeybuf;		/* # of keys in the buffer. */
	static int nextkey;		/* Index of next key in the buffer. */
	static u_char keybuf[256];	/* Key buffer. */
	static u_char *mapoutput;	/* Mapped key return. */
	int ch;
	SEQ *sp;
	int ispartial, nr;

	/* If in the middle of an @ macro, return the next char. */
	if (atkeybuflen) {
		ch = *atkeyp++;
		if (--atkeybuflen == 0)
			free(atkeybuf);
		goto ret;
	}

	/* If returning a mapped key, return the next char. */
	if (mapoutput) {
		ch = *mapoutput;
		if (*++mapoutput == '\0') {
			FF_CLR(ep, F_MSGWAIT);
			mapoutput = NULL;
		}
		goto ret;
	}

	/* Read in more keys if necessary. */
	if (nkeybuf == 0) {
		/* Read the keystrokes. */
		nkeybuf = ttyread(ep, keybuf, sizeof(keybuf), 0);
		nextkey = 0;
		
		/*
		 * If no keys read, then we've reached EOF of an ex script.
		 * XXX
		 * This is just wrong...
		 */
		if (nkeybuf == 0) {
			FF_SET(ep, F_EXIT_FORCE);
			return(0);
		}
	}

	/*
	 * Check for mapped keys. If get a partial match, copy the current
	 * keys down in memory to maximize the space for new keys, and try
	 * to read more keys to complete the map.  Max map is sizeof(keybuf)
	 * and probably not worth fixing.
	 */
	if (flags & (GB_MAPINPUT | GB_MAPCOMMAND) &&
	    seqstart(keybuf[nextkey])) {
retry:		sp = seq_find(&keybuf[nextkey], nkeybuf,
		    flags & GB_MAPCOMMAND ? COMMAND : INPUT, &ispartial);
		if (ispartial) {
			if (sizeof(keybuf) == nkeybuf)
				ep->msg(ep, M_ERROR,
				    "Partial map is too long.");
			else {
				memmove(&keybuf[nextkey], keybuf, nkeybuf);
				nextkey = 0;
				nr = ttyread(ep, keybuf + nkeybuf,
				    sizeof(keybuf) - nkeybuf,
				    (int)LVAL(O_KEYTIME));
				if (nr) {
					nkeybuf += nr;
					goto retry;
				}
			}
		} else if (sp != NULL) {
			nkeybuf -= sp->ilen;
			nextkey += sp->ilen;
			mapoutput = sp->output;
			FF_SET(ep, F_MSGWAIT);
			ch = *mapoutput++;
			goto ret;
		}
	}

	--nkeybuf;
	ch = keybuf[nextkey++];

	/*
	 * XXX
	 * No NULL's for now.
	 */
	if (ch == '\0')
		ch = 'A' & 0x1f;

	/*
	 * O_BEAUTIFY eliminates all control characters except tab,
	 * newline, form-feed and escape.
	 */
ret:	if (flags & GB_BEAUTIFY && ISSET(O_BEAUTIFY)) {
		if (isprint(ch) || special[ch] == K_ESCAPE ||
		    special[ch] == K_FORMFEED || special[ch] == K_NL ||
		    special[ch] == K_TAB)
			return (ch);
		bell(ep);
		return (getkey(ep, flags));
	}
	return (ch);
}

static int __check_sig_winch;				/* GLOBAL */
static int __set_sig_winch;				/* GLOBAL */
static void onwinch __P((int));

static int
ttyread(ep, buf, len, time)
	EXF *ep;
	u_char *buf;		/* where to store the characters */
	int len;		/* max characters to read */
	int time;		/* max tenth seconds to read */
{
	static enum { NOTSET, YES, NO } isfromtty = NOTSET;
	static fd_set rd;
	struct timeval t, *tp;
	int nr, sval;

	/* Set up SIGWINCH handler. */
	if (__set_sig_winch == 0) {
		(void)signal(SIGWINCH, onwinch);
		__set_sig_winch = 1;
	}

	/*
	 * Set if reading from a tty or not on the first entry.  Zero out
	 * the file descriptor array.
	 */
	if (isfromtty == NOTSET) {
		FD_ZERO(&rd);
		isfromtty = isatty(STDIN_FILENO) ? YES : NO;
	}

	/*
	 * If reading from a file or pipe, never timeout.  This
	 * also affects the way that EOF is detected.
	 */
	if (isfromtty == NO) {
		if ((nr = read(STDIN_FILENO, buf, len)) == 0)
			FF_SET(ep, F_EXIT_FORCE);
		return (0);
	}

	/* Compute the timeout value. */
	if (time) {
		t.tv_sec = time / 10;
		t.tv_usec = (time % 10) * 100000L;
		tp = &t;
	} else
		tp = NULL;

	/* Select until characters become available, and then read them. */
	FD_SET(STDIN_FILENO, &rd);

	for (;;) {
		sval = select(1, &rd, NULL, NULL, tp);
		switch (sval) {
		case -1:			/* Error. */
			/* It's okay to be interrupted. */
			if (errno == EINTR) {
				check_sigwinch(ep);
				break;
			}
			ep->msg(ep, M_ERROR,
			    "Terminal read error: %s", strerror(errno));
			return (0);
		case 0:				/* Timeout. */
			return (0);
		default:			/* Read or EOF. */
			if ((nr = read(STDIN_FILENO, buf, len)) == 0)
				FF_SET(ep, F_EXIT_FORCE);
			return (nr);
		}
	}
	/* NOTREACHED */
}

/*
 * onwinch --
 *	Handle SIGWINCH.
 */
void
onwinch(signo)
	int signo;
{
	__check_sig_winch = 1;
}

/*
 * check_sigwinch --
 *	Check for window size change event.   Done here because it's
 *	the only place we block.
 */
static void
check_sigwinch(ep)
	EXF *ep;
{
	sigset_t bmask, omask;

	while (__check_sig_winch == 1) {
		sigemptyset(&bmask);
		sigaddset(&bmask, SIGWINCH);
		(void)sigprocmask(SIG_BLOCK, &bmask, &omask);

		set_window_size(ep, 0);
		SF_SET(ep, S_RESIZE);
		if (FF_ISSET(ep, F_MODE_VI)) {
			(void)scr_update(ep);
			refresh();
		}
		__check_sig_winch = 0;

		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	}
}
