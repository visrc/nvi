/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 8.27 1993/11/29 14:14:53 bostic Exp $ (Berkeley) $Date: 1993/11/29 14:14:53 $";
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
#include "seq.h"

static int keycmp __P((const void *, const void *));

/*
 * XXX
 * THIS REQUIRES THAT ALL SCREENS SHARE A TERMINAL TYPE.
 */
typedef struct _tklist {
	char *ts;			/* Key's termcap string. */
	char *output;			/* Corresponding vi command. */
	char *name;			/* Name. */
} TKLIST;
static TKLIST const tklist[] = {
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
 * XXX
 * THIS REQUIRES THAT ALL SCREENS SHARE A SPECIAL KEY SET.
 */
typedef struct _keylist {
	u_char	value;			/* Special value. */
	CHAR_T	ch;			/* Key. */
} KEYLIST;
static KEYLIST keylist[] = {
	{K_CARAT,	   '^'},
	{K_CNTRLR,	'\022'},
	{K_CNTRLT,	'\024'},
	{K_CNTRLZ,	'\032'},
	{K_COLON,	   ':'},
	{K_CR,		  '\r'},
	{K_ESCAPE,	'\033'},
	{K_FORMFEED,	  '\f'},
	{K_NL,		  '\n'},
	{K_RIGHTBRACE,	   '}'},
	{K_RIGHTPAREN,	   ')'},
	{K_TAB,		  '\t'},
	{K_VEOF,	'\004'},
	{K_VERASE,	  '\b'},
	{K_VKILL,	'\025'},
	{K_VLNEXT,	'\026'},
	{K_VWERASE,	'\027'},
	{K_ZERO,	   '0'},
};

/*
 * term_init --
 *	Initialize the special key lookup table, and the special keys
 *	defined by the terminal's termcap entry.
 */
int
term_init(sp)
	SCR *sp;
{
	extern CHNAME const asciiname[];	/* XXX */
	GS *gp;
	KEYLIST *kp;
	TKLIST const *tkp;
	cc_t ch;
	int cnt;
	char *sbp, *t, buf[2 * 1024], sbuf[128];

	/*
	 * XXX
	 * 8-bit, ASCII only, for now.  Recompilation should get you
	 * any 8-bit character set, as long as nul isn't a character.
	 */
	gp = sp->gp;
	gp->cname = asciiname;			/* XXX */

	/* Set keys found in the termios structure. */
#define	TERMSET(name, val) {						\
	if ((ch = gp->original_termios.c_cc[name]) != _POSIX_VDISABLE)	\
		for (kp = keylist;; ++kp)				\
			if (kp->value == (val)) {			\
				kp->ch = ch;				\
				break;					\
			}						\
}
	TERMSET(VEOF, K_VEOF);
	TERMSET(VERASE, K_VERASE);
	TERMSET(VKILL, K_VKILL);
	TERMSET(VLNEXT, K_VLNEXT);
	TERMSET(VWERASE, K_VWERASE);

	/* Sort the special key list. */
	qsort(keylist, sizeof(keylist), sizeof(keylist[0]), keycmp);

	/* Initialize the fast lookup table. */
	if ((gp->special_key =
	    calloc(MAX_FAST_KEY + 1, sizeof(u_char))) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	for (gp->max_special = 0,
	    kp = keylist, cnt = sizeof(keylist); cnt--; ++kp) {
		if (gp->max_special < kp->value)
			gp->max_special = kp->value;
		if (kp->ch <= MAX_FAST_KEY)
			gp->special_key[kp->ch] = kp->value;
	}
	
	/* Set key sequences found in the termcap entry. */
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

	for (tkp = tklist; tkp->name != NULL; ++tkp) {
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
		if (seq_set(sp, tkp->name, t, tkp->output, SEQ_COMMAND, 0))
			return (1);
	}
	return (0);
}

/*
 * There are two sets of input buffers used by ex/vi.  The first is the input
 * from the user, either via the tty or an initial command supplied with the
 * ex or edit commands or similar mechanism.  The second is the the keys from
 * the first after mapping has been done.  Also, executable buffer expansions
 * are pushed into this second buffer.  The reason that there are two is that
 * we only want to flush the latter buffer if a command fails, i.e. if the user
 * enters "@a1G", we don't want to flush the "1G" keys because "@a" failed.
 */ 

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

/* Remove characters from the head of a queue. */
#define	QREM_HEAD(q, len) {						\
	if (((q)->cnt -= len) == 0)					\
		(q)->next = 0;						\
	else								\
		(q)->next += len;					\
}
/* Remove characters from the tail of a queue. */
#define	QREM_TAIL(q, len) {						\
	if (((q)->cnt -= len) == 0)					\
		(q)->next = 0;						\
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
	CH *chp;
	u_int flags;
{
	enum input rval;
	struct timeval t;
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
			chp->ch = NOT_DIGIT_CH;
			chp->value = 0;
			chp->flags = 0;
			return (INP_OK);
		}
		QREM_HEAD(keyp, 1);
		goto ret;
	}

	/*
	 * Read in more keys if none in the queue.  Since no timeout is
	 * requested, s_key_read will either return an error or will read
	 * some number of characters.
	 */
	if (ttyp->cnt == 0) {
		gp->key_cnt = 0;
		if (rval = sp->s_key_read(sp, &nr, NULL))
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
		t.tv_sec = O_VAL(sp, O_KEYTIME) / 10;
		t.tv_usec = (O_VAL(sp, O_KEYTIME) % 10) * 100000L;
newmap:		ch = ttyp->buf[ttyp->next];
		if (ch < MAX_BIT_SEQ && !bit_test(gp->seqb, ch))
			goto nomap;
remap:		qp = seq_find(sp, NULL, &ttyp->buf[ttyp->next], ttyp->cnt,
		    LF_ISSET(TXT_MAPCOMMAND) ? SEQ_COMMAND : SEQ_INPUT,
		    &ispartial);

		/* If get a partial match, keep trying. */
		if (ispartial) {
			if (rval = sp->s_key_read(sp, &nr, &t))
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
			chp->ch = NOT_DIGIT_CH;
			chp->value = 0;
			chp->flags = 0;
			return (INP_OK);
		}

		QREM_HEAD(ttyp, qp->ilen);
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
		chp->ch = NOT_DIGIT_CH;
		chp->value = 0;
		chp->flags = 0;
		return (INP_OK);
	}
	QREM_HEAD(ttyp, 1);

#define	MAX_KEYS_WITHOUT_READ	1000
ret:	if (++gp->key_cnt > MAX_KEYS_WITHOUT_READ) {
rec_err:	gp->key_cnt = 0;
		TERM_FLUSH(keyp);
		TERM_FLUSH(ttyp);
		msgq(sp, M_ERR,
		    "Error: command expansion too long; keys flushed.");
		return (INP_ERR);
	}
		
	/* Fill in the return information. */
	chp->ch = ch;
	chp->flags = 0;
	chp->value = term_key_val(sp, ch);

	/*
	 * O_BEAUTIFY eliminates all control characters except
	 * escape, form-feed, newline and tab.
	 */
	if (isprint(ch) ||
	    !LF_ISSET(TXT_BEAUTIFY) || !O_ISSET(sp, O_BEAUTIFY) ||
	    chp->value == K_ESCAPE || chp->value == K_FORMFEED ||
	    chp->value == K_NL || chp->value == K_TAB)
		return (INP_OK);

	goto loop;
}

/*
 * term_user_key --
 *	Get the next key, but require the user enter one.
 */
enum input
term_user_key(sp, chp)
	SCR *sp;
	CH *chp;
{
	enum input rval;
	struct timeval t;
	CHAR_T ch;
	IBUF *ttyp;
	int nr;

	/*
	 * Read any keys the user has waiting.  There's an obvious
	 * race condition, but it's quite short.
	 */
	t.tv_sec = 0;
	t.tv_usec = 1;
	if (rval = sp->s_key_read(sp, &nr, &t))
		return (rval);

	/* Read another key. */
	if (rval = sp->s_key_read(sp, &nr, NULL))
		return (rval);
			
	/* Fill in the return information. */
	ttyp = sp->gp->tty;
	chp->ch = ttyp->buf[ttyp->next + (ttyp->cnt - 1)];
	chp->flags = 0;
	chp->value = term_key_val(sp, chp->ch);

	QREM_TAIL(ttyp, 1);
	return (INP_OK);
}

/*
 * __term_key_val --
 *	Fill in the value for a key.  This routine is the backup
 *	for the term_key_val() macro.
 */
int
__term_key_val(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	KEYLIST k, *kp;

	k.ch = ch;
	kp = bsearch(&k, keylist, sizeof(keylist), sizeof(keylist[0]), keycmp);
	return (kp == NULL ? 0 : kp->value);
}

static int
keycmp(ap, bp)
	const void *ap, *bp;
{
	return (((KEYLIST *)ap)->ch - ((KEYLIST *)bp)->ch);
}
