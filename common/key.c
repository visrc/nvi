/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 5.53 1993/04/06 11:36:34 bostic Exp $ (Berkeley) $Date: 1993/04/06 11:36:34 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"

static void	check_sigwinch __P((SCR *));
static void	onwinch __P((int));
static int	ttyread __P((SCR *, char *, int, int));

/*
 * key_special --
 *	Initialize the special array and input buffers.  The special array
 *	has a value for each special character that we can use in a switch
 *	statement.
 */
int
key_special(sp)
	SCR *sp;
{
	/* Keys that are treated specially. */
	sp->special['^'] = K_CARAT;
	sp->special['\004'] = K_CNTRLD;
	sp->special['\022'] = K_CNTRLR;
	sp->special['\032'] = K_CNTRLZ;
	sp->special['\r'] = K_CR;
	sp->special['\033'] = K_ESCAPE;
	sp->special['\f'] = K_FORMFEED;
	sp->special['\n'] = K_NL;
	sp->special['\t'] = K_TAB;
	sp->special['\t'] = K_TAB;
	sp->special[sp->gp->original_termios.c_cc[VERASE]] = K_VERASE;
	sp->special[sp->gp->original_termios.c_cc[VKILL]] = K_VKILL;
	sp->special[sp->gp->original_termios.c_cc[VLNEXT]] = K_VLNEXT;
	sp->special[sp->gp->original_termios.c_cc[VWERASE]] = K_VWERASE;
	sp->special['0'] = K_ZERO;

	return (0);
}

/*
 * gb_inc
 *	Increase the size of the gb buffers.
 */
int
gb_inc(sp)
	SCR *sp;
{
	sp->gb_len += 256;
	if ((sp->gb_cb = realloc(sp->gb_cb, sp->gb_len)) == NULL ||
	    (sp->gb_qb = realloc(sp->gb_qb, sp->gb_len)) == NULL ||
	    (sp->gb_wb = realloc(sp->gb_wb, sp->gb_len)) == NULL) {
			msgq(sp, M_ERROR,
			    "Input too long: %s.", strerror(errno));
			if (sp->gb_cb != NULL)
				free(sp->gb_cb);
			if (sp->gb_qb != NULL)
				free(sp->gb_qb);
			if (sp->gb_wb != NULL)
				free(sp->gb_wb);
			sp->gb_cb = NULL;
			sp->gb_qb = NULL;
			sp->gb_wb = NULL;
			sp->gb_len = 0;
			return (1);
		}
	return (0);
}

/*
 * getkey --
 *	This function reads in a keystroke, as well as handling
 *	mapped keys and executed cut buffers.
 */
int
getkey(sp, flags)
	SCR *sp;
	u_int flags;			/* GB_MAPCOMMAND, GB_MAPINPUT */
{
	int ch;
	SEQ *qp;
	int ispartial, nr;

	/* If in the middle of an @ macro, return the next char. */
	if (sp->atkey_len) {
		ch = *sp->atkey_cur++;
		if (--sp->atkey_len == 0)
			free(sp->atkey_buf);
		goto ret;
	}

	/* If returning a mapped key, return the next char. */
	if (sp->mappedkey) {
		ch = *sp->mappedkey;
		if (*++sp->mappedkey == '\0') {
			F_CLR(sp, S_UPDATE_SCREEN);
			sp->mappedkey = NULL;
		}
		goto ret;
	}

	/* Read in more keys if necessary. */
	if (sp->nkeybuf == 0) {
		/* Read the keystrokes. */
		sp->nkeybuf = ttyread(sp, sp->keybuf, sizeof(sp->keybuf), 0);
		sp->nextkey = 0;
		
		/*
		 * If no keys read, then we've reached EOF of an ex script.
		 * XXX
		 * This is just wrong...
		 */
		if (sp->nkeybuf == 0) {
			F_SET(sp, S_EXIT_FORCE);
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
	    sp->seq[sp->keybuf[sp->nextkey]]) {
retry:		qp = seq_find(sp, &sp->keybuf[sp->nextkey], sp->nkeybuf,
		    flags & GB_MAPCOMMAND ? SEQ_COMMAND : SEQ_INPUT,
		    &ispartial);
		if (ispartial) {
			if (sizeof(sp->keybuf) == sp->nkeybuf)
				msgq(sp, M_ERROR, "Partial map is too long.");
			else {
				memmove(&sp->keybuf[sp->nextkey],
				    sp->keybuf, sp->nkeybuf);
				sp->nextkey = 0;
				nr = ttyread(sp, sp->keybuf + sp->nkeybuf,
				    sizeof(sp->keybuf) - sp->nkeybuf,
				    (int)O_VAL(sp, O_KEYTIME));
				if (nr) {
					sp->nkeybuf += nr;
					goto retry;
				}
			}
		} else if (qp != NULL) {
			sp->nkeybuf -= qp->ilen;
			sp->nextkey += qp->ilen;
			sp->mappedkey = qp->output;
			F_SET(sp, S_UPDATE_SCREEN);
			ch = *sp->mappedkey++;
			goto ret;
		}
	}

	--sp->nkeybuf;
	ch = sp->keybuf[sp->nextkey++];

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
ret:	if (flags & GB_BEAUTIFY && O_ISSET(sp, O_BEAUTIFY)) {
		if (isprint(ch) || sp->special[ch] == K_ESCAPE ||
		    sp->special[ch] == K_FORMFEED || sp->special[ch] == K_NL ||
		    sp->special[ch] == K_TAB)
			return (ch);
		sp->bell(sp);
		return (getkey(sp, flags));
	}
	return (ch);
}

static int __check_sig_winch;				/* XXX GLOBAL */
static int __set_sig_winch;				/* XXX GLOBAL */

static int
ttyread(sp, buf, len, time)
	SCR *sp;
	char *buf;		/* where to store the characters */
	int len;		/* max characters to read */
	int time;		/* max tenth seconds to read */
{
	struct timeval t, *tp;
	int nr, sval;

	/* Set up SIGWINCH handler. */
	if (__set_sig_winch == 0) {
		(void)signal(SIGWINCH, onwinch);
		__set_sig_winch = 1;
	}

	/*
	 * If reading from a file or pipe, never timeout.  This
	 * also affects the way that EOF is detected.
	 */
	if (!F_ISSET(sp, S_ISFROMTTY)) {
		if ((nr = read(STDIN_FILENO, buf, len)) == 0)
			F_SET(sp, S_EXIT_FORCE);
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
	FD_SET(STDIN_FILENO, &sp->rdfd);
	for (;;) {
		sval = select(1, &sp->rdfd, NULL, NULL, tp);
		switch (sval) {
		case -1:			/* Error. */
			/* It's okay to be interrupted. */
			if (errno == EINTR) {
				check_sigwinch(sp);
				break;
			}
			msgq(sp, M_ERROR,
			    "Terminal read error: %s", strerror(errno));
			return (0);
		case 0:				/* Timeout. */
			return (0);
		default:			/* Read or EOF. */
			if ((nr = read(STDIN_FILENO, buf, len)) == 0)
				F_SET(sp, S_EXIT_FORCE);
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
check_sigwinch(sp)
	SCR *sp;
{
	sigset_t bmask, omask;

	while (__check_sig_winch == 1) {
		sigemptyset(&bmask);
		sigaddset(&bmask, SIGWINCH);
		(void)sigprocmask(SIG_BLOCK, &bmask, &omask);

		set_window_size(sp, 0);
		F_SET(sp, S_RESIZE);
		if (F_ISSET(sp, S_MODE_VI)) {
			/*
			 * XXX
			 * This code needs an EXF structure!!
			 * (void)scr_update(sp);
			 */
			refresh();
		}
		__check_sig_winch = 0;

		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	}
}
