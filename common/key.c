/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.31 1992/10/29 14:42:53 bostic Exp $ (Berkeley) $Date: 1992/10/29 14:42:53 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "vi.h"
#include "exf.h"
#include "options.h"
#include "seq.h"
#include "screen.h"
#include "term.h"
#include "extern.h"

u_char special[UCHAR_MAX];		/* Special characters. */

u_long cblen;				/* Buffer lengths. */
u_char *qb;				/* Quote buffer. */
static u_char *cb;			/* Return buffer. */
static u_char *wb;			/* Widths buffer. */

static int	key_inc __P((void));
static void	modeline __P((int));
static void	position __P((int));
static int	ttyread __P((u_char *, int, int));

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

	/* Start off with some memory. */
	(void)gb_inc();
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
			bell();
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
int
gb(prompt, storep, lenp, flags)
	int prompt;
	u_char **storep;
	size_t *lenp;
	u_int flags;
	
{
	register int ch, cnt, quoted;
	register u_char *p;
	size_t clen, col, len;
	char *dp, oct[5];

	col = 0;

	if (mode == MODE_VI)
		MOVE(SCREENSIZE(curf), 0);

	/* Display any prompt. */
	if (prompt) {
		if (mode == MODE_VI)
			addch(prompt);
		else {
			(void)putchar(prompt);
			(void)fflush(stdout);
		}
		++col;
	}
	if (mode == MODE_VI) {
		clrtoeol();
		refresh();
	}

	/* Read in a line. */
	p = flags & GB_OFF ? cb + 1 : cb;
	for (len = quoted = 0;;) {
		if (len >= cblen && gb_inc())
			return (1);
			
		ch = getkey(quoted ? 0 : flags & (GB_MAPCOMMAND | GB_MAPINPUT));
		if (quoted)
			goto insch;

		switch(special[ch]) {
		case K_ESCAPE:
			if (!(flags & GB_ESC))
				goto insch;
			/* FALLTHROUGH */
		case K_CR:
		case K_NL:
			if (flags & GB_NL) {
				*p++ = '\n';
				if (len >= cblen && gb_inc())
					return (1);
			}
			if (mode != MODE_VI) {
				(void)putchar('\n');
				(void)fflush(stdout);
			}
			goto done;
		case K_VERASE:
			if (!len) {
				if (flags & GB_BS) {
					*storep = NULL;
					return (0);
				}
				break;
			}
			--p;
			--len;
			if (mode == MODE_VI) {
				col -= wb[len];
				MOVE(SCREENSIZE(curf), col);
				clrtoeol();
				refresh();
			} else {
				for (cnt = wb[len]; cnt > 0; --cnt, --col)
					(void)printf("\b \b");
				(void)fflush(stdout);
			}
			break;
		case K_VKILL:
			if (!len) {
				bell();
				break;
			}
			if (mode == MODE_VI) {
				if (prompt) {
					col = len = 1;
					MOVE(SCREENSIZE(curf), 1);
				} else {
					col = len = 0;
					MOVE(SCREENSIZE(curf), 0);
				}
				clrtoeol();
				refresh();
			} else {
				while (len)
					for (cnt = wb[--len];
					    cnt > 0; --cnt, --col)
						(void)printf("\b \b");
				(void)fflush(stdout);
			}
			p = cb;
			break;
		case K_VLNEXT:
			if (mode == MODE_VI) {
				addbytes("^", 1);
				MOVE(SCREENSIZE(curf), col);
				refresh();
			} else {
				(void)putchar('^');
				(void)putchar('\b');
				(void)fflush(stdout);
			}
			quoted = 1;
			break;
		case K_VWERASE:
			if (!len) {
				bell();
				break;
			}
			if (mode == MODE_VI) {
				while (len && isspace(*--p))
					col -= wb[--len];
				for (; len && !isspace(*p); --p)
					col -= wb[--len];
				MOVE(SCREENSIZE(curf), col);
				clrtoeol();
				refresh();
			} else {
				while (len && isspace(*--p))
					for (cnt = wb[--len];
					    cnt > 0; --cnt, --col)
						(void)printf("\b \b");
				for (; len && !isspace(*p); --p)
					for (cnt = wb[--len];
					    cnt > 0; --cnt, --col)
						(void)printf("\b \b");
				(void)fflush(stdout);
			}
			if (len)
				++p;
			break;
		default:
insch:			if (quoted) {
				QSET(len);
				quoted = 0;
			}
			/* Add & echo the char. */
			if (ch == '\t') {
				clen = 
				    LVAL(O_TABSTOP) - (col % LVAL(O_TABSTOP));
				dp = "          ";
			} else {
				clen = asciilen[ch];
				dp = asciiname[ch];
			}
			/*
			 * XXX
			 * We limit vi command lines to the screen width.
			 * Should handle longer lines for complex commands.
			 */
			if (col == curf->cols) {
				if (mode == MODE_VI) {
					bell();
					break;
				}
				(void)putchar('\n');
				col = 0;
			}
			if (mode == MODE_VI)
				addbytes(dp, clen);
			else
				(void)printf("%.*s", dp, clen);

			if (mode == MODE_VI)
				refresh();
			else
				(void)fflush(stdout);

			*p++ = ch;
			col += wb[len++] = clen;
			break;
		}
	}

done:	*p = '\0';
	if (lenp)
		*lenp = p - cb;
	*storep = cb;
	return (0);
}

/*
 * getkey --
 *	This function reads in a keystroke, as well as handling mapped keys
 *	and executed cut buffers.
 */
int
getkey(flags)
	u_int flags;			/* GB_MAPCOMMAND, GB_MAPINPUT */
{
	static int nkeybuf;		/* # of keys in the buffer. */
	static int nextkey;		/* Index of next key in the buffer. */
	static u_char keybuf[256];	/* Key buffer. */
	static u_char *mapoutput;	/* Mapped key return. */
	int ch;
	SEQ *sp;
	int inuse, ispartial, nr;

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
			(void)file_stop(curf, 0);
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

	/*
	 * XXX
	 * No NULL's for now.
	 */
	return (ch == '\0' ? 'A' & 0x1f : ch);
}

static int
ttyread(buf, len, time)
	u_char *buf;		/* where to store the characters */
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

	/* Select until characters become available, and then read them. */
	FD_SET(STDIN_FILENO, &rd);
	for (;;)
		switch (select(1, &rd, NULL, NULL, tp)) {
		case -1:
			/* It's okay to be interrupted. */
			if (errno == EINTR)
				break;
			msg("Terminal read error: %s", strerror(errno));
			return (0);
		case 0:
			/* Timeout. */
			return (0);
		default:
			/* Read it. */
			return (read(STDIN_FILENO, buf, len));
		}
}
