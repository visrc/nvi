/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 8.24 1993/11/22 10:01:25 bostic Exp $ (Berkeley) $Date: 1993/11/22 10:01:25 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"

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

static KLIST const klist[] = {
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
	KLIST const *kp;
	cc_t ch;
	char *sbp, *t, buf[2 * 1024], sbuf[128];

	/* Keys that are treated specially. */
	sp->special['^'] = K_CARAT;
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
	if ((ch = sp->gp->original_termios.c_cc[VEOF]) == _POSIX_VDISABLE)
		ch = '\004';
	sp->special[ch] = K_VEOF;
	if ((ch = sp->gp->original_termios.c_cc[VERASE]) == _POSIX_VDISABLE)
		ch = '\b';
	sp->special[ch] = K_VERASE;
	if ((ch = sp->gp->original_termios.c_cc[VKILL]) == _POSIX_VDISABLE)
		ch = '\025';
	sp->special[ch] = K_VKILL;
	if ((ch = sp->gp->original_termios.c_cc[VLNEXT]) == _POSIX_VDISABLE)
		ch = '\026';
	sp->special[ch] = K_VLNEXT;
	if ((ch = sp->gp->original_termios.c_cc[VWERASE]) == _POSIX_VDISABLE)
		ch = '\027';
	sp->special[ch] = K_VWERASE;
	sp->special['0'] = K_ZERO;

	/* Special terminal keys, from the termcap entry. */
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
term_push(sp, ibp, s, len)
	SCR *sp;
	IBUF *ibp;
	char *s;
	size_t len;
{
	size_t nlen;

	/* If we have room, stuff the keys into the buffer. */
	if (len <= ibp->next) {
		ibp->next -= len;
		ibp->cnt += len;
		memmove(ibp->buf + ibp->next, s, len);
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
	if (len == 1)
		ibp->buf[TERM_PUSH_SHIFT] = *s;
	else
		memmove(ibp->buf + TERM_PUSH_SHIFT, s, len);
	ibp->next = TERM_PUSH_SHIFT;
	ibp->cnt += len;
	return (0);
}

/* Remove characters from a queue. */
#define	QREM(q, len) {							\
	if (((q)->cnt -= len) == 0)					\
		(q)->next = 0;						\
	else								\
		(q)->next += len;					\
}

/*
 * term_key --
 *	Get the next key.
 *
 * !!!
 * The flag TXT_MAPNODIGIT probably needs some explanation.  First, the idea
 * of mapping keys is that one or more keystrokes behave like a function key.
 * What's going on is that vi is reading a number, and the character following
 * the number may or may not be mapped (TXT_MAPCOMMAND).  For example, if the
 * user is entering the z command, a valid command is "z40+", and we don't want
 * to map the '+', i.e. if '+' is mapped to "xxx", we don't want to change it
 * into "z40xxx".  However, if the user is entering "35x", we want to put all
 * of the characters through the mapping code.
 *
 * Historical practice is a bit muddled here.  (Surprise!)  It always permitted
 * mapping digits as long as they weren't the first character of the map, e.g.
 * ":map ^A1 xxx" was okay.  It also permitted the mapping of the digits 1-9
 * (the digit 0 was a special case as it doesn't indicate the start of a count)
 * as the first character of the map, but then ignored those mappings.  While
 * it's probably stupid to map digits, vi isn't your mother.
 *
 * The way this works is that the TXT_MAPNODIGIT causes term_key to return the
 * end-of-digit without "looking" at the next character, i.e. leaving it as the
 * user entered it.  Presumably, the next term_key call will tell us how the
 * user wants it handled.
 *
 * There is one more complication.  Users might map keys to digits, and, as
 * it's described above, the commands "map g 1G|d2g" would return the keys
 * "d2<end-of-digits>1G", when the user probably wanted "d21<end-of-digits>G".
 * So, if a map starts off with a digit we continue as before, otherwise, we
 * pretend that we haven't mapped the character and return <end-of-digits>.
 *
 * Now that that's out of the way, let's talk about Energizer Bunny macros.
 * It's easy to create macros that expand to a loop, e.g. map x 3x.  It's
 * fairly easy to detect this example, because it's all internal to term_key.
 * If we're expanding a macro and it gets big enough, at some point we can
 * assume it's looping and kill it.  The examples that are tough are the ones
 * where the parser is involved, e.g. map x "ayyx"byy.  We do an expansion
 * on 'x', and get "ayyx"byy.  We then return the first 4 characters, and then
 * find the looping macro again.  There is no way that we can detect this
 * without doing a full parse of the command, because the character that might
 * cause the loop (in this case 'x') may be a literal character, e.g. the map
 * map x "ayy"xyy"byy is perfectly legal and won't cause a loop.
 *
 * Historic vi tried to detect looping macros by disallowing obvious cases in
 * the map command, maps that that ended with the same letter as they started
 * (which wrongly disallowed map x 'x), and detecting macros that expanded too
 * many times before keys were returned to the command parser.  It didn't get
 * many of the tricky cases right, however, and it was certainly possible to
 * create macros that ran forever.  Finally, changes made before vi realized
 * that the macro was recursing were left in place.
 *
 * We handle the problem by returning no more than MAX_KEYS_WITHOUT_READ keys
 * without reading from the terminal.  If we hit the boundary, we return an
 * error and flush both terminal buffers.  This handles all of the cases, with
 * the exception of macros that loop without returning a character.  We simply
 * count those cases and error if there are more than MAX_MACRO_EXP expansions.
 *
 * XXX
 * The final issue is recovery.  It would be possible to undo all of the work
 * that was done by the macro if we entered a record into the log so that we
 * knew when the macro started, and, in fact, this might be worth doing at some
 * point.  For now we just flush the keys and leave any changes made in place.
 */
enum input
term_key(sp, chp, flags)
	SCR *sp;
	CHAR_T *chp;
	u_int flags;
{
	enum input rval;
	GS *gp;
	IBUF *keyp, *ttyp;
	SEQ *qp;
	int ch, ispartial, ml_cnt, nr;

	/* If we have expanded keys, return the next one. */
	gp = sp->gp;
	keyp = gp->key;
	ttyp = gp->tty;
loop:	if (keyp->cnt) {
		ch = keyp->buf[keyp->next];
		if (LF_ISSET(TXT_MAPNODIGIT) && !isdigit(ch)) {
			*chp = NOT_DIGIT_CH;
			return (INP_OK);
		}
		QREM(keyp, 1);
		goto ret;
	}

	/*
	 * Read in more keys if none in the queue.  Since no timeout
	 * is requested, s_key_read will either return 1 or will read
	 * some number of characters.
	 */
	if (ttyp->cnt == 0) {
		gp->key_cnt = 0;
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

	/* Check for mapped keys.  */
	if (LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT)) {
		ml_cnt = 0;
newmap:		ch = ttyp->buf[ttyp->next];
		if (ch < MAX_BIT_SEQ && !bit_test(gp->seqb, ch))
			goto nomap;
remap:		qp = seq_find(sp, &ttyp->buf[ttyp->next], ttyp->cnt,
		    LF_ISSET(TXT_MAPCOMMAND) ? SEQ_COMMAND : SEQ_INPUT,
		    &ispartial);

		/* If get a partial match, keep trying. */
		if (ispartial) {
			if (rval = sp->s_key_read(sp, &nr, 1))
				return (rval);
			if (nr)
				goto remap;
			goto nomap;
		}

		if (qp == NULL)
			goto nomap;

#define	MAX_MACRO_EXP	40
		if (++ml_cnt > MAX_MACRO_EXP)
			goto rec_err;

		if (LF_ISSET(TXT_MAPNODIGIT) && !isdigit(qp->output[0])) {
			*chp = NOT_DIGIT_CH;
			return (INP_OK);
		}

		QREM(ttyp, qp->ilen);
		if (O_ISSET(sp, O_REMAP)) {
			if (term_push(sp, ttyp, qp->output, qp->olen))
				goto err;
			goto newmap;
		}
		if (term_push(sp, keyp, qp->output, qp->olen)) {
err:			msgq(sp, M_SYSERR, "Error: key deleted");
			return (INP_ERR);
		}
		goto loop;
	}

nomap:	ch = ttyp->buf[ttyp->next];
	if (LF_ISSET(TXT_MAPNODIGIT) && !isdigit(ch)) {
		*chp = NOT_DIGIT_CH;
		return (INP_OK);
	}
	QREM(ttyp, 1);

#define	MAX_KEYS_WITHOUT_READ	1000
ret:	if (++gp->key_cnt > MAX_KEYS_WITHOUT_READ) {
rec_err:	gp->key_cnt = 0;
		TERM_FLUSH(keyp);
		TERM_FLUSH(ttyp);
		msgq(sp, M_ERR,
		    "Error: command expansion too long; keys flushed.");
		return (INP_ERR);
	}
		
	/*
	 * O_BEAUTIFY eliminates all control characters except
	 * escape, form-feed, newline and tab.
	 */
	if (LF_ISSET(TXT_BEAUTIFY) && O_ISSET(sp, O_BEAUTIFY)) {
		if (isprint(ch) ||
		    sp->special[ch] == K_ESCAPE ||
		    sp->special[ch] == K_FORMFEED ||
		    sp->special[ch] == K_NL ||
		    sp->special[ch] == K_TAB) {
			*chp = ch;
			return (INP_OK);
		}
		goto loop;
	}
	*chp = ch;
	return (INP_OK);
}
