/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.16 1992/04/28 13:48:51 bostic Exp $ (Berkeley) $Date: 1992/04/28 13:48:51 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <termios.h>
#include <limits.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "vi.h"
#include "options.h"
#include "seq.h"
#include "tty.h"
#include "extern.h"

#define	K_CR		1
#define	K_ESCAPE	2
#define	K_NL		3
#define	K_VERASE	4
#define	K_VKILL		5
#define	K_VLNEXT	6
#define	K_VWERASE	7
static char special[UCHAR_MAX];		/* Special characters. */

u_long cblen;				/* Buffer lengths. */
char *qb;				/* Quote buffer. */
static char *cb;			/* Return buffer. */
static char *wb;			/* Widths buffer. */

static int	key_inc __P((void));
static void	modeline __P((int));
static void	position __P((int));

/*
 * gb_init --
 *	Initialize the special array.  Basically, this array has a value for
 *	each special character that we can use in a switch statement.  This
 *	speeds up lookup and normal insertion tremendously.
 */
void
gb_init()
{
	struct termios t;

	bzero(special, sizeof(special));

	if (tcgetattr(STDIN_FILENO, &t))
		return;

	/* User-defined keys. */
	special[t.c_cc[VERASE]] = K_VERASE;
	special[t.c_cc[VKILL]] = K_VKILL;
	special[t.c_cc[VLNEXT]] = K_VLNEXT;
	special[t.c_cc[VWERASE]] = K_VWERASE;

	/* Standard keys. */
	special['\n'] = K_NL;
	special['\r'] = K_CR;
	special['\033'] = K_ESCAPE;
}

/*
 * gb_inc
 *	Increase the size of the gb buffers.
 */
int
gb_inc()
{
	cblen += 256;
	if ((cb = realloc(cb, cblen)) == NULL ||
	    (qb = realloc(qb, cblen)) == NULL ||
	    (wb = realloc(wb, cblen)) == NULL) {
			msg("Input too long: %s.", strerror(errno));
			if (cb)
				free(cb);
			if (qb)
				free(qb);
			if (wb)
				free(wb);
			cb = qb = wb = NULL;
			cblen = 0;
			return (1);
		}
	return (0);
}

/*
 * gb --
 *	Fill a buffer from the terminal.
 */
char *
gb(prompt, flags)
	int prompt;
	u_int flags;
{
	register int ch, cnt, col, len, quoted;
#ifndef NO_DIGRAPH
	int erased;			/* 0, or first char of a digraph. */
#endif
	register char *p;
	char oct[5];

	col = 0;

	/* Display any prompt. */
	if (prompt) {
		(void)putchar(prompt);
		(void)fflush(stdout);
		++col;
	}

	/* Read in a line. */
#ifndef NO_DIGRAPH
	erased = 0;
#endif
	p = flags & GB_OFF ? cb + 1 : cb;
	for (len = quoted = 0;;) {
		if (len >= cblen && gb_inc())
			return (NULL);
			
		ch = getkey(quoted ? 0 : WHEN_EX);

#ifndef NO_DIGRAPH
		if (ISSET(O_DIGRAPH) && erased != 0 && ch != '\b') {
			ch = digraph(erased, ch);
			erased = 0;
		}
#endif
		if (quoted)
			goto insch;

		switch(special[ch]) {
		case K_ESCAPE:
			if (!(flags & GB_ESC))
				goto insch;
			/* FALLTHROUGH */
		case K_NL:
		case K_CR:
			if (flags & GB_NL) {
				*p++ = '\n';
				if (len >= cblen && gb_inc())
					return (NULL);
			}
			if (flags & GB_NLECHO) {
				(void)putchar('\n');
				(void)fflush(stdout);
			}
			goto done;
		case K_VERASE:
			if (!len) {
				if (flags & GB_BS)
					return (NULL);
				break;
			}
			--len;
			--p;
#ifndef NO_DIGRAPH
			erased = *p;
#endif
			for (cnt = wb[len]; cnt > 0; --cnt, --col)
				(void)printf("\b \b");
			(void)fflush(stdout);
			break;
		case K_VKILL:
			if (!len)
				break;
			while (len)
				for (cnt = wb[--len]; cnt > 0; --cnt, --col)
					(void)printf("\b \b");
			(void)fflush(stdout);
			p = cb;
			break;
		case K_VLNEXT:
			(void)putchar('^');
			(void)putchar('\b');
			(void)fflush(stdout);
			quoted = 1;
			break;
		case K_VWERASE:
			if (!len)
				break;
			while (len && isspace(*--p))
				for (cnt = wb[--len]; cnt > 0; --cnt, --col)
					(void)printf("\b \b");
			for (; len && !isspace(*p); --p)
				for (cnt = wb[--len]; cnt > 0; --cnt, --col)
					(void)printf("\b \b");
			(void)fflush(stdout);
			if (len)
				++p;
			break;
		case 0:
insch:			if (quoted) {
				QSET(len);
				quoted = 0;
			}
/*
 * XXX
 * If cross boundary, should probably change repaint flags?
 */
#define	WCHECK(ch) { \
	if (col == COLS) { \
		(void)putchar('\n'); \
		col = 0; \
	} \
	(void)putchar(ch); \
	++col; \
}
			/* Add & echo the char. */
			if (ch == '\t') {
				wb[len] =
				    LVAL(O_TABSTOP) - (col % LVAL(O_TABSTOP));
				for (cnt = wb[len]; cnt > 0; --cnt, ++col)
					WCHECK(' ');
			} else if (isprint(ch)) {
				WCHECK(ch);
				wb[len] = 1;
				++col;
			} else if (ch & 0x80) {
				wb[len] =
				    snprintf(oct, sizeof(oct), "\\%03o", ch);
				for (cnt = 0; cnt < wb[len]; ++cnt, ++col)
					WCHECK(oct[cnt]);
			} else if (ch > 0 && ch < ' ') {
				WCHECK('^');
				WCHECK(ch + 0x40);
				col += wb[len] = 2;
			}
			(void)fflush(stdout);
			*p++ = ch;
			++len;
			break;
		}
	}

done:	*p = '\0';
	return (cb);
}

/*
 * getkey --
 *	This function reads in a keystroke, as well as handling mapped keys
 *	and executed cut buffers.
 */
int
getkey(when)
	int when;			/* Which bits must be ON? */
{
	static int nkeybuf;		/* # of keys in the buffer. */
	static int nextkey;		/* Index of next key in the buffer. */
	static char keybuf[256];	/* Key buffer. */
	static char *mapoutput;		/* Mapped key return. */
	int ch;
	SEQ *sp;
	int inuse, ispartial, nr;

#ifdef DEBUG
	watch();
#endif

	/*
	 * If this key is needed for delay between multiple error messages,
	 * then reset the msgwait flag and drop any mapped key sequence.
	 * XXX
	 * Why drop mapped key??
	 */
#ifdef notdef
	if (msgwaiting && endmsg() &&
	    (when == WHEN_MSG || when == WHEN_VIINP || when == WHEN_VIREP)) {
		if (when == WHEN_MSG) {
			addstr("[More...]");
			mapoutput = NULL;
		}
		refresh();
	}
#endif

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
		nkeybuf = ttyread(keybuf, sizeof(keybuf), 0);
		nextkey = 0;
		
		/*
		 * If no keys read, then we've reached EOF of an ex script.
		 * XXX
		 * This should return to somewhere else.
		 */
		if (nkeybuf == 0) {
			tmpabort(1);
			move(LINES - 1, 0);
			clrtoeol();
			refresh();
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
	if (seqstart(keybuf[nextkey])) {
retry:		sp = seq_find(&keybuf[nextkey], nkeybuf,
		    when & WHEN_VICMD ? COMMAND : INPUT, &ispartial);
		if (ispartial) {
			if (sizeof(keybuf) == nkeybuf)
				msg("Partial map is too long.");
			else {
				bcopy(keybuf, &keybuf[nextkey], nkeybuf);
				nextkey = 0;
				nr = ttyread(keybuf + nkeybuf,
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

	/* Translate weird erase key to '\b'. */
	if (ch == erasechar() && when != 0)
		return('\b');
	else if (ch == '\0')
		return ('A' & 0x1f);
	return (ch);
}

int
ttyread(buf, len, time)
	char *buf;		/* where to store the characters */
	int len;		/* max characters to read */
	int time;		/* max tenth seconds to read */
{
	static enum { NOTSET, YES, NO } isfromtty = NOTSET;
	static fd_set rd;
	struct timeval t, *tp;

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

	/*
	 * Select until characters become available, and then read them.  Try
	 * to handle SIGWINCH -- if a signal arrives during the select call,
	 * adjust the O_COLUMNS and O_LINES variables, and fake a control-L.
	 */
	FD_SET(STDIN_FILENO, &rd);
	for (;;)
		switch (select(1, &rd, NULL, NULL, tp)) {
		case -1:
			/*
			 * Assume we got an EINTR because of SIGWINCH, and
			 * pretend the user hit ^L.
			 *
			 * XXX
			 * Should check EINTR, not just make the assumption.
			 */
			if (LVAL(O_LINES) != LINES || LVAL(O_COLUMNS) != COLS) {
				LVAL(O_LINES) = LINES;
				LVAL(O_COLUMNS) = COLS;
				if (mode != MODE_EX) {
					*buf = ctrl('L');
					return (1);
				}
			}
			break;
		case 0:
			/* Timeout. */
			return (0);
		default:
			/* Read it. */
			return (read(STDIN_FILENO, buf, len));
		}
}
