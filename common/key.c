/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 8.10 1993/09/30 12:02:14 bostic Exp $ (Berkeley) $Date: 1993/09/30 12:02:14 $";
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
 * term_key --
 *	Get the next key.
 */
enum input
term_key(sp, chp, flags)
	SCR *sp;
	CHAR_T *chp;
	u_int flags;
{
	enum input rval;
	IBUF *keyp, *ttyp;
	SEQ *qp;
	int ch, ispartial, nr;

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
	keyp = sp->gp->key;
	ttyp = sp->gp->tty;
kloop:	if (keyp->cnt) {
		ch = keyp->buf[keyp->next++];
		if (--keyp->cnt == 0)
			keyp->next = 0;
		goto beauty;
	}

	/*
	 * Read in more keys if none in the queue.  Since no timeout
	 * is requested, s_key_read will either return 1 or will read
	 * some number of characters.
	 */
	if (ttyp->cnt == 0) {
		if (rval = sp->s_key_read(sp, &nr, 0))
			return (rval);
		/*
		 * If there's something on the mode line that we wanted
		 * the user to see, they just entered a character so we
		 * can presume they saw it.  Clear the bit before doing
		 * the refresh, the refresh routine is probably checking.
		 */
		if (F_ISSET(sp, S_UPDATE_MODE)) {
			F_CLR(sp, S_UPDATE_MODE);
			if (sp->s_refresh(sp, sp->ep))
				return (INP_ERR);
		}
	}

	/*
	 * Check for mapped keys. If get a partial match, copy the current
	 * keys down in memory to maximize the space for new keys, and try
	 * to read more keys to complete the map.
	 */
	if (LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT)) {
mloop:		qp = seq_find(sp, &ttyp->buf[ttyp->next], ttyp->cnt,
		    LF_ISSET(TXT_MAPCOMMAND) ? SEQ_COMMAND : SEQ_INPUT,
		    &ispartial);
		if (!ispartial) {
			if (qp == NULL)
				goto nomap;
			ttyp->cnt -= qp->ilen;
			if (ttyp->cnt == 0)
				ttyp->next = 0;
			else
				ttyp->next += qp->ilen;

			if (O_ISSET(sp, O_REMAP)) {
				if (term_push(sp, ttyp, qp->output, qp->olen))
					goto err;
				goto mloop;
			} else
				if (term_push(sp, keyp, qp->output, qp->olen)) {
err:					msgq(sp, M_ERR,
					    "Error: keys flushed: %s.",
					    strerror(errno));
					TERM_KEY_FLUSH(sp);
				}
			goto kloop;
		}
		if (rval = sp->s_key_read(sp, &nr, 1))
			return (rval);
		if (nr)
			goto mloop;
	}

nomap:	ch = ttyp->buf[ttyp->next++];
	if (--ttyp->cnt == 0)
		ttyp->next = 0;

	/*
	 * O_BEAUTIFY eliminates all control characters except
	 * escape, form-feed, newline and tab.
	 */
beauty:	if (LF_ISSET(TXT_BEAUTIFY) && O_ISSET(sp, O_BEAUTIFY)) {
		if (isprint(ch) ||
		    sp->special[ch] == K_ESCAPE ||
		    sp->special[ch] == K_FORMFEED ||
		    sp->special[ch] == K_NL ||
		    sp->special[ch] == K_TAB) {
			*chp = ch;
			return (INP_OK);
		}
		goto kloop;
	}
	*chp = ch;
	return (INP_OK);
}
