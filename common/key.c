/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 8.5 1993/08/27 11:43:41 bostic Exp $ (Berkeley) $Date: 1993/08/27 11:43:41 $";
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
#include "recover.h"

/*
 * There are two sets of input buffers used by ex/vi.  The first is the input
 * from the user, either via the tty or an initial command supplied with the
 * ex or edit commands or similar mechanism.  The second is the the keys from
 * the first after mapping has been done.  Also, executable buffer expansions
 * are pushed into this second buffer.  The reason that there are two is that
 * we only want to flush the latter buffer if a command fails, i.e. if the user
 * enters "@a1G", we don't want to flush the "1G" keys because "@a" failed.
 */ 

static void	change_window_size __P((void));
static int	term_read __P((SCR *, int *, int));

typedef struct _klist {
	char *ts;				/* Key's termcap string. */
	char *output;				/* Corresponding vi command. */
	char *name;				/* Name. */
} KLIST;

static KLIST klist[] = {
	{"kA",    "O", "insert line"},
	{"kD",    "x", "delete character"},
	{"kd",    "j", "cursor down"},
	{"kE",    "D", "delete to eol"},
	{"kF", "\004", "scroll down"},
	{"kH",    "$", "go to eol"},
	{"kh",    "^", "go to sol"},
	{"kI",    "i", "insert at cursor"},
	{"kL",   "dd", "delete line"},
	{"kl",    "h", "cursor left"},
	{"kN", "\006", "page down"},
	{"kP", "\002", "page up"},
	{"kR", "\025", "scroll up"},
	{"kS",	 "dG", "delete to end of screen"},
	{"kr",    "l", "cursor right"},
	{"ku",    "k", "cursor up"},
	{NULL},
};

/*
 * term_init --
 *	Initialize the special array and special keys.  The special array
 *	has a value for each special character that we can use in a switch
 *	statement.
 */
int
term_init(sp)
	SCR *sp;
{
	KLIST *kp;
	char *sbp, *t, buf[2 * 1024], sbuf[128];

	/* Keys that are treated specially. */
	sp->special['^'] = K_CARAT;
	sp->special['\004'] = K_CNTRLD;
	sp->special['\022'] = K_CNTRLR;
	sp->special['\024'] = K_CNTRLT;
	sp->special['\032'] = K_CNTRLZ;
	sp->special[':'] = K_COLON;
	sp->special['\r'] = K_CR;
	sp->special['\033'] = K_ESCAPE;
	sp->special['\f'] = K_FORMFEED;
	sp->special['\n'] = K_NL;
	sp->special[')'] = K_RIGHTPAREN;
	sp->special['}'] = K_RIGHTBRACE;
	sp->special['\t'] = K_TAB;
	sp->special[sp->gp->original_termios.c_cc[VERASE]] = K_VERASE;
	sp->special[sp->gp->original_termios.c_cc[VKILL]] = K_VKILL;
	sp->special[sp->gp->original_termios.c_cc[VLNEXT]] = K_VLNEXT;
	sp->special[sp->gp->original_termios.c_cc[VWERASE]] = K_VWERASE;
	sp->special['0'] = K_ZERO;

	/*
	 * Special terminal keys.
	 * Get the termcap entry.
	 */
	switch (tgetent(buf, O_STR(sp, O_TERM))) {
	case -1:
		msgq(sp, M_ERR, "%s tgetent: %s.",
		    O_STR(sp, O_TERM), strerror(errno));
		return (1);
	case 0:
		msgq(sp, M_ERR, "%s: unknown terminal type.",
		    O_STR(sp, O_TERM), strerror(errno));
		return (1);
	}

	for (kp = klist; kp->name != NULL; ++kp) {
		sbp = sbuf;
		if ((t = tgetstr(kp->ts, &sbp)) == NULL)
			continue;
		if (seq_set(sp, kp->name, t, kp->output, SEQ_COMMAND, 0))
			return (1);
	}
	return (0);
}

/*
 * term_push --
 *	Push keys onto the front of a buffer.
 */
int
term_push(sp, ibp, push, len)
	SCR *sp;
	IBUF *ibp;
	char *push;
	size_t len;
{
	size_t nlen;

	/* If we have room, stuff the keys into the buffer. */
	if (len <= ibp->next) {
		ibp->next -= len;
		ibp->cnt += len;
		memmove(ibp->buf + ibp->next, push, len);
		return (0);
	}

	/* Get enough space plus a little extra. */
	nlen = ibp->cnt + len;
	BINC(sp, ibp->buf, ibp->len, nlen + 64);

	/*
	 * If there is currently characters in the buffer,
	 * shift them up, and leave a little extra room.
	 */
#define	TERM_PUSH_SHIFT	20
	if (ibp->cnt)
		memmove(ibp->buf + len + TERM_PUSH_SHIFT,
		    ibp->buf + ibp->next, ibp->cnt);
	memmove(ibp->buf + TERM_PUSH_SHIFT, push, len);
	ibp->next = TERM_PUSH_SHIFT;
	ibp->cnt += len;
	return (0);
}

/*
 * term_waiting --
 *	Return if keys waiting.
 */
int
term_waiting(sp)
	SCR *sp;
{
	struct timeval t;

	if (sp->key.cnt != 0 || sp->tty.cnt != 0)
		return (1);

	t.tv_sec = t.tv_usec = 0;
	FD_SET(STDIN_FILENO, &sp->rdfd);
	return (select(32, &sp->rdfd, NULL, NULL, &t));
}

/*
 * term_key --
 *	This function returns the next key to the calling function.
 */
int
term_key(sp, flags)
	SCR *sp;
	u_int flags;
{
	int ch;
	SEQ *qp;
	int ispartial, nr;

	/*
	 * Sync the recovery file if necessary.  This has nothing to do
	 * with input keys, but it's something that can't be done via
	 * a signal mechanism because of race conditions, and which needs
	 * to be done periodically.  I feel confident that this routine
	 * will be executed, ah, periodically.
	 */
	if (F_ISSET(sp->ep, F_RCV_ALRM)) {
		F_CLR(sp->ep, F_RCV_ALRM);
		(void)rcv_sync(sp, sp->ep);
	}

	/* If we have expanded keys, return the next one. */
kloop:	if (sp->key.cnt) {
		ch = sp->key.buf[sp->key.next++];
		if (--sp->key.cnt == 0)
			sp->key.next = 0;
		goto beauty;
	}

	/*
	 * Read in more keys if none in the queue.  Since no timeout
	 * is requested, term_read will either return 1 or will read
	 * some number of characters.
	 */
	if (sp->tty.cnt == 0 && term_read(sp, &nr, 0))
		return (1);

	/*
	 * Check for mapped keys. If get a partial match, copy the current
	 * keys down in memory to maximize the space for new keys, and try
	 * to read more keys to complete the map.
	 */
	if (LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT)) {
mloop:		qp = seq_find(sp, &sp->tty.buf[sp->tty.next], sp->tty.cnt,
		    LF_ISSET(TXT_MAPCOMMAND) ? SEQ_COMMAND : SEQ_INPUT,
		    &ispartial);
		if (!ispartial) {
			if (qp == NULL)
				goto nomap;
			sp->tty.cnt -= qp->ilen;
			if (sp->tty.cnt == 0)
				sp->tty.next = 0;
			else
				sp->tty.next += qp->ilen;

			if (O_ISSET(sp, O_REMAP)) {
				if (term_push(sp,
				    &sp->tty, qp->output, qp->olen))
					goto err;
				goto mloop;
			} else
				if (term_push(sp,
				    &sp->key, qp->output, qp->olen)) {
err:					msgq(sp, M_ERR,
					    "Error: keys flushed: %s.",
					    strerror(errno));
					TERM_KEY_FLUSH(sp);
				}
			goto kloop;
		}
		if (term_read(sp, &nr, 1))
			return (1);
		if (nr)
			goto mloop;
	}

nomap:	ch = sp->tty.buf[sp->tty.next++];
	if (--sp->tty.cnt == 0)
		sp->tty.next = 0;

	/*
	 * O_BEAUTIFY eliminates all control characters except
	 * escape, form-feed, newline and tab.
	 */
beauty:	if (LF_ISSET(TXT_BEAUTIFY) && O_ISSET(sp, O_BEAUTIFY)) {
		if (isprint(ch) ||
		    sp->special[ch] == K_ESCAPE ||
		    sp->special[ch] == K_FORMFEED ||
		    sp->special[ch] == K_NL ||
		    sp->special[ch] == K_TAB)
			return (ch);
		goto kloop;
	}
	return (ch);
}

/*
 * term_read --
 *	Read characters from the input.
 *
 * XXX
 * If we ever fail to read any characters, we set the forced exit flag
 * and leave.  This is almost certainly wrong.
 */
static int
term_read(sp, nrp, timeout)
	SCR *sp;
	int *nrp, timeout;
{
	struct timeval t, *tp;
	int nr;

	/*
	 * If we're reading less than 20 characters, up the size of the
	 * tty buffer.  This shouldn't ever happen, other than the first
	 * time through, but it's possible if a map were large enough.
	 */
	if (sp->tty.len - sp->tty.cnt < 20)
		BINC(sp, sp->tty.buf, sp->tty.len, sp->tty.len + 64);

	*nrp = 0;

	/*
	 * We're about to block; check for events we should have 
	 * handled.
	 */
	if (F_ISSET(__global_list, G_SIGWINCH))
		change_window_size();

	/*
	 * If reading from a file or pipe, never timeout.
	 * This affects the way that EOF is detected.
	 */
	if (!F_ISSET(sp, S_ISFROMTTY)) {
		if ((nr = read(STDIN_FILENO, sp->tty.buf + sp->tty.cnt,
		    sp->tty.len - sp->tty.cnt)) > 0) {
			sp->tty.cnt += *nrp = nr;
			return (0);
		}
		F_SET(sp, S_EXIT_FORCE);
		return (1);
	}

	/* If there's a time limit on the read, compute the timeout value. */
	if (timeout) {
		t.tv_sec = O_VAL(sp, O_KEYTIME) / 10;
		t.tv_usec = (O_VAL(sp, O_KEYTIME) % 10) * 100000L;
		tp = &t;

		FD_SET(STDIN_FILENO, &sp->rdfd);
	}

	/*
	 * Select until characters become available, and then read them.
	 * If interrupted, it was SIGALRM or SIGWINCH.  SIGALRM only
	 * happens every RCV_PERIOD seconds, so it shouldn't be a problem.
	 * We check signals here because it's the only place we block.
	 */
	for (;;) {
		if (timeout)
			switch (select(32, &sp->rdfd, NULL, NULL, tp)) {
			case -1:		/* Error or interrupt. */
				if (errno == EINTR) {
					change_window_size();
					continue;
				}
				goto err;
			case 0:			/* Timeout. */
				return (0);
			}
		switch (nr = read(STDIN_FILENO,
		    sp->tty.buf + sp->tty.cnt, sp->tty.len - sp->tty.cnt)) {
		case 0:				/* EOF. */
			goto eof;
		case -1:			/* Error or interrupt. */
			if (errno == EINTR) {
				change_window_size();
				continue;
			}
err:			msgq(sp, M_ERR,
			    "Terminal read error: %s", strerror(errno));
eof:			F_SET(sp, S_EXIT_FORCE);
			return (1);
		default:
			sp->tty.cnt += *nrp = nr;
			return (0);
		}
	}
	/* NOTREACHED */
}

/*
 * change_window_size --
 *	Handle window size change event.
 */
static void
change_window_size()
{
	SCR *sp;
	sigset_t bmask, omask;

	while (F_ISSET(__global_list, G_SIGWINCH)) {
		sigemptyset(&bmask);
		sigaddset(&bmask, SIGWINCH);
		(void)sigprocmask(SIG_BLOCK, &bmask, &omask);

		/*
		 * XXX
		 * This doesn't handle split screens!  If the screen grows,
		 * can simply add the extra space to the bottom screen.  If
		 * it shrinks, though, may not even be able to display all
		 * of the screens.
		 */
		sp = __global_list->scrhdr.next;
		F_SET(sp, S_RESIZE);
		set_window_size(sp, 0, 1);
		(void)sp->s_refresh(sp, sp->ep);

		for (; sp != (SCR *)&__global_list->scrhdr; sp = sp->next)
			F_SET(sp, S_REFORMAT);

		F_CLR(__global_list, G_SIGWINCH);
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	}
}
