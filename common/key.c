/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: key.c,v 10.2 1995/05/05 18:46:01 bostic Exp $ (Berkeley) $Date: 1995/05/05 18:46:01 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "common.h"

static int	v_event_append __P((SCR *, CHAR_T *, size_t, u_int));
static int	v_event_grow __P((SCR *, int));
static int	v_key_cmp __P((const void *, const void *));
static void	v_key_termios __P((GS *, int, int));
static void	v_sig_sync __P((SCR *, int));
static void	v_sync __P((SCR *, int));

/*
 * !!!
 * Historic vi always used:
 *
 *	^D: autoindent deletion
 *	^H: last character deletion
 *	^W: last word deletion
 *	^Q: quote the next character (if not used in flow control).
 *	^V: quote the next character
 *
 * regardless of the user's choices for these characters.  The user's erase
 * and kill characters worked in addition to these characters.  Nvi wires
 * down the above characters, but in addition permits the VEOF, VERASE, VKILL
 * and VWERASE characters described by the user's termios structure.
 *
 * Ex was not consistent with this scheme, as it historically ran in tty
 * cooked mode.  This meant that the scroll command and autoindent erase
 * characters were mapped to the user's EOF character, and the character
 * and word deletion characters were the user's tty character and word
 * deletion characters.  This implementation makes it all consistent, as
 * described above for vi.
 *
 * !!!
 * This requires that all screens share a special key set.
 */
KEYLIST keylist[] = {
	{K_BACKSLASH,	  '\\'},	/*  \ */
	{K_CARAT,	   '^'},	/*  ^ */
	{K_CNTRLD,	'\004'},	/* ^D */
	{K_CNTRLR,	'\022'},	/* ^R */
	{K_CNTRLT,	'\024'},	/* ^T */
	{K_CNTRLZ,	'\032'},	/* ^Z */
	{K_COLON,	   ':'},	/*  : */
	{K_CR,		  '\r'},	/* \r */
	{K_ESCAPE,	'\033'},	/* ^[ */
	{K_FORMFEED,	  '\f'},	/* \f */
	{K_HEXCHAR,	'\030'},	/* ^X */
	{K_NL,		  '\n'},	/* \n */
	{K_RIGHTBRACE,	   '}'},	/*  } */
	{K_RIGHTPAREN,	   ')'},	/*  ) */
	{K_TAB,		  '\t'},	/* \t */
	{K_VERASE,	  '\b'},	/* \b */
	{K_VKILL,	'\025'},	/* ^U */
	{K_VLNEXT,	'\021'},	/* ^Q */
	{K_VLNEXT,	'\026'},	/* ^V */
	{K_VWERASE,	'\027'},	/* ^W */
	{K_ZERO,	   '0'},	/*  0 */

#define	ADDITIONAL_CHARACTERS	4
	{K_NOTUSED, 0},			/* VEOF, VERASE, VKILL, VWERASE */
	{K_NOTUSED, 0},
	{K_NOTUSED, 0},
	{K_NOTUSED, 0},
};
static int nkeylist =
    (sizeof(keylist) / sizeof(keylist[0])) - ADDITIONAL_CHARACTERS;

/*
 * v_key_init --
 *	Initialize the special key lookup table.
 *
 * PUBLIC: int v_key_init __P((SCR *));
 */
int
v_key_init(sp)
	SCR *sp;
{
	CHAR_T ch;
	GS *gp;
	KEYLIST *kp;
	int cnt;

	/*
	 * XXX
	 * 8-bit only, for now.  Recompilation should get you any 8-bit
	 * character set, as long as nul isn't a character.
	 */
	(void)setlocale(LC_ALL, "");
	v_key_ilookup(sp);

	gp = sp->gp;
#ifdef VEOF
	v_key_termios(gp, VEOF, K_CNTRLD);
#endif
#ifdef VERASE
	v_key_termios(gp, VERASE, K_VERASE);
#endif
#ifdef VKILL
	v_key_termios(gp, VKILL, K_VKILL);
#endif
#ifdef VWERASE
	v_key_termios(gp, VWERASE, K_VWERASE);
#endif

	/* Sort the special key list. */
	qsort(keylist, nkeylist, sizeof(keylist[0]), v_key_cmp);

	/* Initialize the fast lookup table. */
	for (gp->max_special = 0, kp = keylist, cnt = nkeylist; cnt--; ++kp) {
		if (gp->max_special < kp->value)
			gp->max_special = kp->value;
		if (kp->ch <= MAX_FAST_KEY)
			gp->special_key[kp->ch] = kp->value;
	}

	/* Find a non-printable character to use as a message separator. */
	for (ch = 1; ch <= MAX_CHAR_T; ++ch)
		if (!isprint(ch)) {
			gp->noprint = ch;
			break;
		}
	if (ch != gp->noprint) {
		msgq(sp, M_ERR, "090|No non-printable character found");
		return (1);
	}
	return (0);
}

/*
 * v_key_termios --
 *	Set keys found in the termios structure.
 *
 * VEOF, VERASE and VKILL are required by POSIX 1003.1-1990, VWERASE is
 * a 4BSD extension.  We've left some open slots in the keylist table,
 * and if these values exist, we put them into place.  Note, they may reset
 * (or duplicate) values already in the table, so we check for that first.
 */
static void
v_key_termios(gp, name, val)
	GS *gp;
	int name, val;
{
	KEYLIST *kp;
	cc_t ch;

	if (!F_ISSET(gp, G_TERMIOS_SET))
		return;
	if ((ch = gp->original_termios.c_cc[name]) == _POSIX_VDISABLE)
		return;

	/* Check for duplication. */
	for (kp = keylist; kp->value != K_NOTUSED; ++kp)
		if (kp->ch == ch) {
			kp->value = val;
			return;
		}
	/* Add a new entry. */
	if (kp->value == K_NOTUSED) {
		keylist[nkeylist].ch = ch;
		keylist[nkeylist].value = val;
		++nkeylist;
	}
}

/*
 * v_key_ilookup --
 *	Build the fast-lookup key display array.
 *
 * PUBLIC: void v_key_ilookup __P((SCR *));
 */
void
v_key_ilookup(sp)
	SCR *sp;
{
	CHAR_T ch, *p, *t;
	GS *gp;
	size_t len;

	for (gp = sp->gp, ch = 0; ch <= MAX_FAST_KEY; ++ch)
		for (p = gp->cname[ch].name, t = __v_key_name(sp, ch),
		    len = gp->cname[ch].len = sp->clen; len--;)
			*p++ = *t++;
}

/*
 * __v_key_len --
 *	Return the length of the string that will display the key.
 *	This routine is the backup for the KEY_LEN() macro.
 *
 * PUBLIC: size_t __v_key_len __P((SCR *, ARG_CHAR_T));
 */
size_t
__v_key_len(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	(void)__v_key_name(sp, ch);
	return (sp->clen);
}

/*
 * __v_key_name --
 *	Return the string that will display the key.  This routine
 *	is the backup for the KEY_NAME() macro.
 *
 * PUBLIC: CHAR_T *__v_key_name __P((SCR *, ARG_CHAR_T));
 */
CHAR_T *
__v_key_name(sp, ach)
	SCR *sp;
	ARG_CHAR_T ach;
{
	static const CHAR_T hexdigit[] = "0123456789abcdef";
	static const CHAR_T octdigit[] = "01234567";
	CHAR_T ch, *chp, mask;
	size_t len;
	int cnt, shift;

	ch = ach;

	/* See if the character was explicitly declared printable or not. */
	if ((chp = O_STR(sp, O_PRINT)) != NULL)
		for (; *chp != '\0'; ++chp)
			if (*chp == ch)
				goto pr;
	if ((chp = O_STR(sp, O_NOPRINT)) != NULL)
		for (; *chp != '\0'; ++chp)
			if (*chp == ch)
				goto nopr;

	/*
	 * Historical (ARPA standard) mappings.  Printable characters are left
	 * alone.  Control characters less than '\177' are represented as '^'
	 * followed by the character offset from the '@' character in the ASCII
	 * map.  '\177' is represented as '^' followed by '?'.
	 *
	 * XXX
	 * The following code depends on the current locale being identical to
	 * the ASCII map from '\100' to '\076' (\076 since that's the largest
	 * character for which we can offset from '@' and get something that's
	 * a printable character in ASCII.  I'm told that this is a reasonable
	 * assumption...
	 *
	 * XXX
	 * This code will only work with CHAR_T's that are multiples of 8-bit
	 * bytes.
	 *
	 * XXX
	 * NB: There's an assumption here that all printable characters take
	 * up a single column on the screen.  This is not always correct.
	 */
	if (isprint(ch)) {
pr:		sp->cname[0] = ch;
		len = 1;
		goto done;
	}
nopr:	if (ch <= '\076' && iscntrl(ch)) {
		sp->cname[0] = '^';
		sp->cname[1] = ch == '\177' ? '?' : '@' + ch;
		len = 2;
	} else if (O_ISSET(sp, O_OCTAL)) {
#define	BITS	(sizeof(CHAR_T) * 8)
#define	SHIFT	(BITS - BITS % 3)
#define	TOPMASK	(BITS % 3 == 2 ? 3 : 1) << (BITS - BITS % 3)
		sp->cname[0] = '\\';
		sp->cname[1] = octdigit[(ch & TOPMASK) >> SHIFT];
		shift = SHIFT - 3;
		for (len = 2, mask = 7 << (SHIFT - 3),
		    cnt = BITS / 3; cnt-- > 0; mask >>= 3, shift -= 3)
			sp->cname[len++] = octdigit[(ch & mask) >> shift];
	} else {
		sp->cname[0] = '\\';
		sp->cname[1] = 'x';
		for (len = 2, chp = (u_int8_t *)&ch,
		    cnt = sizeof(CHAR_T); cnt-- > 0; ++chp) {
			sp->cname[len++] = hexdigit[(*chp & 0xf0) >> 4];
			sp->cname[len++] = hexdigit[*chp & 0x0f];
		}
	}
done:	sp->cname[sp->clen = len] = '\0';
	return (sp->cname);
}

/*
 * __v_key_val --
 *	Fill in the value for a key.  This routine is the backup
 *	for the KEY_VAL() macro.
 *
 * PUBLIC: int __v_key_val __P((SCR *, ARG_CHAR_T));
 */
int
__v_key_val(sp, ch)
	SCR *sp;
	ARG_CHAR_T ch;
{
	KEYLIST k, *kp;

	k.ch = ch;
	kp = bsearch(&k, keylist, nkeylist, sizeof(keylist[0]), v_key_cmp);
	return (kp == NULL ? K_NOTUSED : kp->value);
}

/*
 * v_event_push --
 *	Push keys onto the front of the buffer.
 *
 * There is a single input buffer in ex/vi.  Characters are read into the
 * end of the buffer by the terminal input routines, and pushed onto the
 * front of the buffer by various other functions in ex/vi.  Each key has
 * an associated flag value, which indicates if it has already been quoted,
 * if it is the result of a mapping or an abbreviation, as well as a count
 * of the number of times it has been mapped.
 *
 * PUBLIC: int v_event_push __P((SCR *, CHAR_T *, size_t, u_int));
 */
int
v_event_push(sp, s, nchars, flags)
	SCR *sp;
	CHAR_T *s;			/* Characters. */
	size_t nchars;			/* Number of chars. */
	u_int flags;			/* CH_* flags. */
{
	EVENT *evp;
	GS *gp;
	size_t total;

	/* If we have room, stuff the keys into the buffer. */
	gp = sp->gp;
	if (nchars <= gp->i_next ||
	    (gp->i_event != NULL && gp->i_cnt == 0 && nchars <= gp->i_nelem)) {
		if (gp->i_cnt != 0)
			gp->i_next -= nchars;
		goto copy;
	}

	/*
	 * If there are currently keys in the queue, shift them up, leaving
	 * some extra room.  Get enough space plus a little extra.
	 */
#define	TERM_PUSH_SHIFT	30
	total = gp->i_cnt + gp->i_next + nchars + TERM_PUSH_SHIFT;
	if (total >= gp->i_nelem && v_event_grow(sp, MAX(total, 64)))
		return (1);
	if (gp->i_cnt)
		MEMMOVE(gp->i_event + TERM_PUSH_SHIFT + nchars,
		    gp->i_event + gp->i_next, gp->i_cnt);
	gp->i_next = TERM_PUSH_SHIFT;

	/* Put the new keys into the queue. */
copy:	gp->i_cnt += nchars;
	for (evp = gp->i_event + gp->i_next; nchars--; ++evp) {
		evp->e_event = E_CHARACTER;
		evp->e_c = *s++;
		evp->e_value = KEY_VAL(sp, evp->e_c);
		F_INIT(&evp->e_ch, flags);
	}
	return (0);
}

/*
 * v_event_append --
 *	Append keys onto the tail of the buffer.
 */
static int
v_event_append(sp, s, nchars, flags)
	SCR *sp;
	CHAR_T *s;			/* Characters. */
	size_t nchars;			/* Number of chars. */
	u_int flags;			/* CH_* flags. */
{
	EVENT *evp;
	GS *gp;

	gp = sp->gp;

	/* Grow the buffer if necessary. */
	if (gp->i_event == NULL ||
	    nchars > gp->i_nelem - (gp->i_next + gp->i_cnt))
		v_event_grow(sp, MAX(nchars, 64));

	evp = gp->i_event + gp->i_next + gp->i_cnt;
	gp->i_cnt += nchars;
	for (; nchars--; ++evp) {
		evp->e_event = E_CHARACTER;
		evp->e_c = *s++;
		evp->e_value = KEY_VAL(sp, evp->e_c);
		F_INIT(&evp->e_ch, flags);
	}
	return (0);
}

/* Remove characters from the queue. */
#define	QREM(len) {							\
	if ((gp->i_cnt -= len) == 0)					\
		gp->i_next = 0;						\
	else								\
		gp->i_next += len;					\
}

/* Remove characters from the queue. */
#define	QREM(len) {							\
	if ((gp->i_cnt -= len) == 0)					\
		gp->i_next = 0;						\
	else								\
		gp->i_next += len;					\
}

/* 
 * v_event_pull --
 *	Return the next character event, if it exists.
 *
 * PUBLIC: int v_event_pull __P((SCR *, EVENT **));
 */
int
v_event_pull(sp, evpp)
	SCR *sp;
	EVENT **evpp;
{
	GS *gp;
	size_t cnt;

	gp = sp->gp;
	if (gp->i_cnt == 0)
		return (0);
	*evpp = &gp->i_event[gp->i_next];
	QREM(1);
	return (1);
}

/*
 * v_event_handler --
 *	Handle the next event.
 *
 * !!!
 * The flag EC_NODIGIT probably needs some explanation.  First, the idea of
 * mapping keys is that one or more keystrokes act like a function key.
 * What's going on is that vi is reading a number, and the character following
 * the number may or may not be mapped (EC_MAPCOMMAND).  For example, if the
 * user is entering the z command, a valid command is "z40+", and we don't want
 * to map the '+', i.e. if '+' is mapped to "xxx", we don't want to change it
 * into "z40xxx".  However, if the user enters "35x", we want to put all of the
 * characters through the mapping code.
 *
 * Historical practice is a bit muddled here.  (Surprise!)  It always permitted
 * mapping digits as long as they weren't the first character of the map, e.g.
 * ":map ^A1 xxx" was okay.  It also permitted the mapping of the digits 1-9
 * (the digit 0 was a special case as it doesn't indicate the start of a count)
 * as the first character of the map, but then ignored those mappings.  While
 * it's probably stupid to map digits, vi isn't your mother.
 *
 * The way this works is that the EC_MAPNODIGIT causes term_key to return the
 * end-of-digit without "looking" at the next character, i.e. leaving it as the
 * user entered it.  Presumably, the next term_key call will tell us how the
 * user wants it handled.
 *
 * There is one more complication.  Users might map keys to digits, and, as
 * it's described above, the commands:
 *
 *	:map g 1G
 *	d2g
 *
 * would return the keys "d2<end-of-digits>1G", when the user probably wanted
 * "d21<end-of-digits>G".  So, if a map starts off with a digit we continue as
 * before, otherwise, we pretend we haven't mapped the character, and return
 * <end-of-digits>.
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
 * (which wrongly disallowed "map x 'x"), and detecting macros that expanded
 * too many times before keys were returned to the command parser.  It didn't
 * get many (most?) of the tricky cases right, however, and it was certainly
 * possible to create macros that ran forever.  And, even if it did figure out
 * what was going on, the user was usually tossed into ex mode.  Finally, any
 * changes made before vi realized that the macro was recursing were left in
 * place.  We recover gracefully, but the only recourse the user has in an
 * infinite macro loop is to interrupt.
 *
 * !!!
 * It is historic practice that mapping characters to themselves as the first
 * part of the mapped string was legal, and did not cause infinite loops, i.e.
 * ":map! { {^M^T" and ":map n nz." were known to work.  The initial, matching
 * characters were returned instead of being remapped.
 *
 * XXX
 * The final issue is recovery.  It would be possible to undo all of the work
 * that was done by the macro if we entered a record into the log so that we
 * knew when the macro started, and, in fact, this might be worth doing at some
 * point.  Given that this might make the log grow unacceptably (consider that
 * cursor keys are done with maps), for now we leave any changes made in place.
 *
 * PUBLIC: int v_event_handler __P((SCR *, EVENT *, u_int32_t *));
 */
int
v_event_handler(sp, evp, tp)
	SCR *sp;
	EVENT *evp;
	u_int32_t *tp;
{
	EVENT re;
	GS *gp;
	SEQ *qp;
	int init_nomap, ispartial, istimeout;

	gp = sp->gp;

	/*
	 * Push character and string event(s) on the queue.  If a timeout
	 * event arrives, it's because a map failed, so return the first
	 * character unmapped.  All other events are handled by the editor
	 * handlers.
	 *
	 * !!!
	 * DO NOT push other types of events on the queue, other parts of
	 * the editor won't like it.
	 */
	istimeout = 0;
	switch (evp->e_event) {
	case E_CHARACTER:
		if (v_event_append(sp, &evp->e_c, 1, 0))
			goto err;
		break;
	case E_STRING:
		if (v_event_append(sp, evp->e_csp, evp->e_len, 0))
			goto err;
		break;
	case E_EOF:
	case E_ERR:
		(void)(F_ISSET(sp, S_EX) ? ex(sp, evp) : vi(sp, evp));
		goto err;
	case E_SIGHUP:
	case E_SIGTERM:
		v_sig_sync(sp, evp->e_event == E_SIGHUP ? SIGHUP : SIGTERM);
		/* NOTREACHED */
	case E_TIMEOUT:
		istimeout = 1;
		break;
	case E_RESIZE:
		/*
		 * The actual changing of the row/column values is done by the
		 * options code, which puts them into the environment, which is
		 * used by curses.  Stupid, but ugly.
		 */
		if (opts_init(sp, NULL, evp->e_lno, evp->e_cno))
			goto err;
		/* FALLTHROUGH */
	case E_INTERRUPT:
	case E_REPAINT:
	case E_SIGCONT:
	case E_START:
	case E_STOP:
		re = *evp;
		if (F_ISSET(sp, S_EX) ? ex(sp, &re) : vi(sp, &re))
			goto err;
		goto ret;
	default:
		abort();
	}

	/*
	 * If EC_INTERRUPT flag set, something was just checking for
	 * interrupts.  Return a timeout event.
	 */
	if (FL_ISSET(gp->ec_flags, EC_INTERRUPT)) {
		re.e_event = E_TIMEOUT;
		if (F_ISSET(sp, S_EX) ? ex(sp, &re) : vi(sp, &re))
			goto err;
		if (FL_ISSET(gp->ec_flags, EC_INTERRUPT))
			goto ret;
	}

	/* As long as there are events on the queue, handle them. */
	while (gp->i_cnt != 0) {
newmap:		evp = &gp->i_event[gp->i_next];

		/*
		 * If the key isn't mappable because
		 *	+ ... already timed out
		 *	+ ... it's not a mappable key
		 *	+ ... neither the command or input map flags are set
		 *	+ ... there are no maps that can apply to it
		 * return it forthwith.
		 */
		if (istimeout || F_ISSET(&evp->e_ch, CH_NOMAP) ||
		    !FL_ISSET(gp->ec_flags, EC_MAPCOMMAND | EC_MAPINPUT) ||
		    evp->e_c < MAX_BIT_SEQ && !bit_test(gp->seqb, evp->e_c))
			goto nomap;

		/* Search the map. */
		qp = seq_find(sp, NULL, evp, NULL, gp->i_cnt,
		    FL_ISSET(gp->ec_flags,
		    EC_MAPCOMMAND) ? SEQ_COMMAND : SEQ_INPUT, &ispartial);

		/*
		 * If get a partial match, get more characters and retry the
		 * map.  If time out without further characters, return the
		 * characters unmapped.
		 *
		 * !!!
		 * <escape> characters are a problem.  Input mode ends with an
		 * <escape>, and cursor keys start with one, so there's an ugly
		 * pause at the end of an input session.  If it's an <escape>,
		 * check for follow-on characters, but timeout after a 10th of
		 * a second.  This loses if users create maps that use <escape>
		 * as the first character, and that aren't entered fast enough.
		 * Since such maps are generally function keys, we're probably
		 * safe.
		 */
		if (ispartial) {
			if (O_ISSET(sp, O_TIMEOUT))
				if (evp->e_value == K_ESCAPE)
					*tp = 100;	/* 1/10th of a sec. */
				else
					*tp = O_VAL(sp, O_KEYTIME) * 100;
			return (0);
		}

		/* If no map, return the character. */
		if (qp == NULL) {
nomap:			if (!isdigit(evp->e_c) &&
			    FL_ISSET(gp->ec_flags, EC_MAPNODIGIT)) {
not_digit:			re.e_c = CH_NOT_DIGIT;
				re.e_value = K_NOTUSED;
				F_INIT(&re.e_ch, 0);
				if (F_ISSET(sp, S_EX) ?
				    ex(sp, evp) : vi(sp, &re))
					goto err;
			}
			re = *evp;
			QREM(1);

			if (F_ISSET(sp, S_EX) ?
			    ex(sp, &re) : vi(sp, &re))
				goto err;
			continue;
		}

		/*
		 * If looking for the end of a digit string, and the first
		 * character of the map is it, pretend we haven't seen the
		 * character.
		 */
		if (FL_ISSET(gp->ec_flags, EC_MAPNODIGIT) &&
		    qp->output != NULL && !isdigit(qp->output[0]))
			goto not_digit;

		/* Find out if the initial segments are identical. */
		init_nomap =
		    !memcmp(&gp->i_event[gp->i_next], qp->output, qp->ilen);

		/* Delete the mapped characters from the queue. */
		QREM(qp->ilen);

		/* If keys mapped to nothing, go get more. */
		if (qp->output == NULL)
			continue;

		/* If remapping characters, push the character on the queue. */
		if (O_ISSET(sp, O_REMAP)) {
			if (init_nomap) {
				if (v_event_push(sp, qp->output + qp->ilen,
				    qp->olen - qp->ilen, CH_MAPPED))
					goto err;
				if (v_event_push(sp,
				    qp->output, qp->ilen, CH_NOMAP | CH_MAPPED))
					goto err;
				goto nomap;
			} else
				if (v_event_push(sp,
				    qp->output, qp->olen, CH_MAPPED))
					goto err;
			goto newmap;
		}

		/* Else, push the characters on the queue and return one. */
		if (v_event_push(sp,
		    qp->output, qp->olen, CH_MAPPED | CH_NOMAP))
			goto err;
	}

ret:	if (FL_ISSET(gp->ec_flags, EC_INTERRUPT))
		*tp = 1;
	return (0);

err:	v_sync(sp, RCV_EMAIL | RCV_ENDSESSION | RCV_PRESERVE);
	return (1);
}

/*
 * v_event_flush --
 *	Flush any flagged keys, returning if any keys were flushed.
 *
 * PUBLIC: int v_event_flush __P((SCR *, u_int));
 */
int
v_event_flush(sp, flags)
	SCR *sp;
	u_int flags;
{
	GS *gp;
	int rval;

	for (rval = 0, gp = sp->gp; gp->i_cnt != 0 &&
	    F_ISSET(&gp->i_event[gp->i_next].e_ch, flags); rval = 1)
		QREM(1);
	return (rval);
}

/*
 * v_event_grow --
 *	Grow the terminal queue.
 */
static int
v_event_grow(sp, add)
	SCR *sp;
	int add;
{
	GS *gp;
	size_t new_nelem, olen;

	gp = sp->gp;
	new_nelem = gp->i_nelem + add;
	olen = gp->i_nelem * sizeof(gp->i_event[0]);
	BINC_RET(sp, gp->i_event, olen, new_nelem * sizeof(gp->i_event[0]));
	gp->i_nelem = olen / sizeof(gp->i_event[0]);
	return (0);
}

/*
 * v_key_cmp --
 *	Compare two keys for sorting.
 */
static int
v_key_cmp(ap, bp)
	const void *ap, *bp;
{
	return (((KEYLIST *)ap)->ch - ((KEYLIST *)bp)->ch);
}

/*
 * v_sig_sync --
 *	Sync the files after a signal.
 */
static void
v_sig_sync(sp, signo)
	SCR *sp;
	int signo;
{
	v_sync(sp,
	    RCV_ENDSESSION | RCV_PRESERVE | (signo == SIGHUP ? RCV_EMAIL : 0));

	/*
	 * Die with the proper exit status.  Don't bother using sigaction(2)
	 * 'cause we want the default behavior.
	 */
	(void)signal(signo, SIG_DFL);
	(void)kill(getpid(), signo);

	/* NOTREACHED */
	exit(1);
}

/*
 * v_sync --
 *	Walk the screen lists, sync'ing files to their backup copies.
 */
static void
v_sync(sp, flags)
	SCR *sp;
	int flags;
{
	GS *gp;

	gp = sp->gp;
	for (sp = gp->dq.cqh_first; sp != (void *)&gp->dq; sp = sp->q.cqe_next)
		rcv_sync(sp, flags);
	for (sp = gp->hq.cqh_first; sp != (void *)&gp->hq; sp = sp->q.cqe_next)
		rcv_sync(sp, flags);
}
