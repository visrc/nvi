/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_term.c,v 8.1 1994/07/15 16:18:47 bostic Exp $ (Berkeley) $Date: 1994/07/15 16:18:47 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
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

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"
#include "svi_screen.h"

/*
 * XXX
 * THIS REQUIRES THAT ALL SCREENS SHARE A TERMINAL TYPE.
 */
typedef struct _tklist {
	char	*ts;			/* Key's termcap string. */
	char	*output;		/* Corresponding vi command. */
	char	*name;			/* Name. */
	u_char	 value;			/* Special value (for lookup). */
} TKLIST;
static TKLIST const c_tklist[] = {	/* Command mappings. */
#ifdef SYSV_CURSES
	{"kil1",	"O",	"insert line"},
	{"kdch1",	"x",	"delete character"},
	{"kcud1",	"j",	"cursor down"},
	{"kel",		"D",	"delete to eol"},
	{"kind",     "\004",	"scroll down"},
	{"kll",		"$",	"go to eol"},
	{"khome",	"^",	"go to sol"},
	{"kich1",	"i",	"insert at cursor"},
	{"kdl1",       "dd",	"delete line"},
	{"kcub1",	"h",	"cursor left"},
	{"knp",	     "\006",	"page down"},
	{"kpp",	     "\002",	"page up"},
	{"kri",	     "\025",	"scroll up"},
	{"ked",	       "dG",	"delete to end of screen"},
	{"kcuf1",	"l",	"cursor right"},
	{"kcuu1",	"k",	"cursor up"},
#else
	{"kA",		"O",	"insert line"},
	{"kD",		"x",	"delete character"},
	{"kd",		"j",	"cursor down"},
	{"kE",		"D",	"delete to eol"},
	{"kF",	     "\004",	"scroll down"},
	{"kH",		"$",	"go to eol"},
	{"kh",		"^",	"go to sol"},
	{"kI",		"i",	"insert at cursor"},
	{"kL",	       "dd",	"delete line"},
	{"kl",		"h",	"cursor left"},
	{"kN",	     "\006",	"page down"},
	{"kP",	     "\002",	"page up"},
	{"kR",	     "\025",	"scroll up"},
	{"kS",	       "dG",	"delete to end of screen"},
	{"kr",		"l",	"cursor right"},
	{"ku",		"k",	"cursor up"},
#endif
	{NULL},
};
static TKLIST const m1_tklist[] = {	/* Input mappings (lookup). */
	{NULL},
};
static TKLIST const m2_tklist[] = {	/* Input mappings (set or delete). */
#ifdef SYSV_CURSES
	{"kcud1",  "\033ja",	"cursor down"},
	{"kcub1",  "\033ha",	"cursor left"},
	{"kcuu1",  "\033ka",	"cursor up"},
	{"kcuf1",  "\033la",	"cursor right"},
#else
	{"kd",	   "\033ja",	"cursor down"},
	{"kl",	   "\033ha",	"cursor left"},
	{"ku",	   "\033ka",	"cursor up"},
	{"kr",	   "\033la",	"cursor right"},
#endif
	{NULL},
};

/*
 * svi_term_init --
 *	Initialize the special keys defined by the termcap/terminfo entry.
 */
int
svi_term_init(sp)
	SCR *sp;
{
	GS *gp;
	KEYLIST *kp;
	SEQ *qp;
	TKLIST const *tkp;
	size_t len;
	int cnt;
	char *sbp, *s, *t, sbuf[1024];

	/* Regardless of the results, it's initialized. */
	F_SET(SVP(sp), SVI_CURSES_INIT);

	/* Command mappings. */
	for (tkp = c_tklist; tkp->name != NULL; ++tkp) {
#ifdef SYSV_CURSES
		if ((t = tigetstr(tkp->ts)) == NULL || t == (char *)-1)
			continue;
#else
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
#endif
		if (seq_set(sp, tkp->name, strlen(tkp->name), t, strlen(t),
		    tkp->output, strlen(tkp->output), SEQ_COMMAND, SEQ_SCREEN))
			return (1);
	}

	/* Input mappings needing to be looked up. */
	for (tkp = m1_tklist; tkp->name != NULL; ++tkp) {
#ifdef SYSV_CURSES
		if ((t = tigetstr(tkp->ts)) == NULL || t == (char *)-1)
			continue;
#else
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
#endif
		for (kp = keylist;; ++kp)
			if (kp->value == tkp->value)
				break;
		if (kp == NULL)
			continue;
		if (seq_set(sp, tkp->name, strlen(tkp->name),
		    t, strlen(t), &kp->ch, 1, SEQ_INPUT, SEQ_SCREEN))
			return (1);
	}

	/* Input mappings that are already set or are text deletions. */
	for (tkp = m2_tklist; tkp->name != NULL; ++tkp) {
#ifdef SYSV_CURSES
		if ((t = tigetstr(tkp->ts)) == NULL || t == (char *)-1)
			continue;
#else
		sbp = sbuf;
		if ((t = tgetstr(tkp->ts, &sbp)) == NULL)
			continue;
#endif
		/*
		 * !!!
		 * Some terminals' <cursor_left> keys send single <backspace>
		 * characters.  This is okay in command mapping, but not okay
		 * in input mapping.  That combination is the only one we'll
		 * ever see, hopefully, so kluge it here for now.
		 */
		if (!strcmp(t, "\b"))
			continue;
		if (tkp->output == NULL) {
			if (seq_set(sp, tkp->name, strlen(tkp->name),
			    t, strlen(t), NULL, 0, SEQ_INPUT, SEQ_SCREEN))
				return (1);
		} else
			if (seq_set(sp, tkp->name, strlen(tkp->name),
			    t, strlen(t), tkp->output, strlen(tkp->output),
			    SEQ_INPUT, SEQ_SCREEN))
				return (1);
	}

	/* Rework any function key mappings. */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next) {
		if (!F_ISSET(qp, SEQ_FUNCMAP))
			continue;
		(void)svi_fmap(sp, qp->stype,
		    qp->input, qp->ilen, qp->output, qp->olen);
	}

	/* Set up the visual bell information. */
	t = sbuf;
	if (tgetstr("vb", &t) != NULL && (len = t - sbuf) != 0) {
		MALLOC_RET(sp, s, char *, len);
		memmove(s, sbuf, len);
		if (SVP(sp)->VB != NULL)
			free(SVP(sp)->VB);
		SVP(sp)->VB = s;
		return (0);
	}

	return (0);
}

/*
 * svi_fmap --
 *	Map a function key.
 */
int
svi_fmap(sp, stype, from, flen, to, tlen)
	SCR *sp;
	enum seqtype stype;
	CHAR_T *from, *to;
	size_t flen, tlen;
{
	char *t, keyname[64];
	size_t nlen;

	/* If the terminal isn't initialized, there's nothing to do. */
	if (!F_ISSET(SVP(sp), SVI_CURSES_INIT))
		return (0);

#ifdef SYSV_CURSES
	(void)snprintf(keyname, sizeof(keyname), "kf%d", atoi(from + 1));
	if ((t = tigetstr(keyname)) == NULL || t == (char *)-1)
		t = NULL;
#else
	{ char *sbp, sbuf[1024];
		(void)snprintf(keyname, sizeof(keyname), "k%d", atoi(from + 1));
		sbp = sbuf;
		t = tgetstr(keyname, &sbp);
	}
#endif
	if (t != NULL) {
		nlen = snprintf(keyname,
		    sizeof(keyname), "function key %d", atoi(from + 1));
		return (seq_set(sp, keyname, nlen, t, strlen(t),
		    to, tlen, stype, SEQ_SCREEN | SEQ_USERDEF));
	}
	msgq(sp, M_ERR, "This terminal has no %s key", from);
	return (1);
}
