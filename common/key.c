/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.40 1993/02/19 11:13:01 bostic Exp $ (Berkeley) $Date: 1993/02/19 11:13:01 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
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

	/* User-defined keys. */
	special[t.c_cc[VERASE]] = K_VERASE;
	special[t.c_cc[VKILL]] = K_VKILL;
	special[t.c_cc[VLNEXT]] = K_VLNEXT;
	special[t.c_cc[VWERASE]] = K_VWERASE;

	/* Standard keys that are treated specially. */
	special['^'] = K_CARAT;
	special['\004'] = K_CNTRLD;
	special['\r'] = K_CR;
	special['\033'] = K_ESCAPE;
	special['\n'] = K_NL;
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
			msg(ep, M_ERROR,
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
		return (ch);
	}

	/* If returning a mapped key, return the next char. */
	if (mapoutput) {
		if (*mapoutput)
			return (*mapoutput++);
		mapoutput = NULL;
	}

	/* Read in more keys if necessary. */
	if (nkeybuf == 0) {
		/* Read the keystrokes. */
		nkeybuf = ttyread(ep, keybuf, sizeof(keybuf), 0);
		nextkey = 0;
		
		/*
		 * If no keys read, then we've reached EOF of an ex script.
		 * XXX
		 * This should return to somewhere else.
		 */
		if (nkeybuf == 0) {
			(void)file_stop(ep, 0);
			if (move(LINES - 1, 0) != ERR) {
				clrtoeol();
				refresh();
			}
			endwin();
			exit(1);
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
				msg(ep, M_ERROR, "Partial map is too long.");
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
			return (*mapoutput++);
		}
	}

	--nkeybuf;
	ch = keybuf[nextkey++];

	/*
	 * XXX
	 * No NULL's for now.
	 */
	return (ch == '\0' ? 'A' & 0x1f : ch);
}

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
	int sval;

	/*
	 * Set if reading from a tty or not on the first entry.  Zero out
	 * the file descriptor array.
	 */
	if (isfromtty == NOTSET) {
		FD_ZERO(&rd);
		isfromtty = isatty(STDIN_FILENO) ? YES : NO;
	}

	/*
	 * If reading from a file or pipe, never timeout.  (This also affects
	 * the way that EOF is detected.)
	 */
	if (isfromtty == NO)
		return (read(STDIN_FILENO, buf, len));

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
		/*
		 * This is the only place we actually wait, so have to handle
		 * asynchronous resizing here.  If resize is scheduled, do it
		 * before selecting.
		 */
		FF_SET(ep, F_READING);
		if (FF_ISSET(ep, F_RESIZE) && mode == MODE_VI) {
			(void)scr_update(ep);
			refresh();
		}
		sval = select(1, &rd, NULL, NULL, tp);
		FF_CLR(ep, F_READING);
		switch (sval) {
		case -1:			/* Error. */
			/* It's okay to be interrupted. */
			if (errno == EINTR)
				break;
			msg(ep, M_ERROR,
			    "Terminal read error: %s", strerror(errno));
			return (0);
		case 0:
			return (0);		/* Timeout. */
		default:
			return (read(STDIN_FILENO, buf, len));
		}
	}
}
