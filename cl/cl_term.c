/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_term.c,v 10.3 1995/06/26 11:05:44 bostic Exp $ (Berkeley) $Date: 1995/06/26 11:05:44 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "common.h"
#include "cl.h"

static int cl_putenv __P((char *));

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
	{NULL},
};
static TKLIST const m1_tklist[] = {	/* Input mappings (lookup). */
	{NULL},
};
static TKLIST const m2_tklist[] = {	/* Input mappings (set or delete). */
	{"kcud1",  "\033ja",	"cursor down"},
	{"kcub1",  "\033ha",	"cursor left"},
	{"kcuu1",  "\033ka",	"cursor up"},
	{"kcuf1",  "\033la",	"cursor right"},
	{NULL},
};

/*
 * cl_term_init --
 *	Initialize the special keys defined by the termcap/terminfo entry.
 *
 * PUBLIC: int cl_term_init __P((SCR *));
 */
int
cl_term_init(sp)
	SCR *sp;
{
	KEYLIST *kp;
	SEQ *qp;
	TKLIST const *tkp;
	char *t;

	/* Command mappings. */
	for (tkp = c_tklist; tkp->name != NULL; ++tkp) {
		if ((t = tigetstr(tkp->ts)) == NULL || t == (char *)-1)
			continue;
		if (seq_set(sp, tkp->name, strlen(tkp->name), t, strlen(t),
		    tkp->output, strlen(tkp->output), SEQ_COMMAND,
		    SEQ_NOOVERWRITE | SEQ_SCREEN))
			return (1);
	}

	/* Input mappings needing to be looked up. */
	for (tkp = m1_tklist; tkp->name != NULL; ++tkp) {
		if ((t = tigetstr(tkp->ts)) == NULL || t == (char *)-1)
			continue;
		for (kp = keylist;; ++kp)
			if (kp->value == tkp->value)
				break;
		if (kp == NULL)
			continue;
		if (seq_set(sp, tkp->name, strlen(tkp->name), t, strlen(t),
		    &kp->ch, 1, SEQ_INPUT, SEQ_NOOVERWRITE | SEQ_SCREEN))
			return (1);
	}

	/* Input mappings that are already set or are text deletions. */
	for (tkp = m2_tklist; tkp->name != NULL; ++tkp) {
		if ((t = tigetstr(tkp->ts)) == NULL || t == (char *)-1)
			continue;
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
			    t, strlen(t), NULL, 0,
			    SEQ_INPUT, SEQ_NOOVERWRITE | SEQ_SCREEN))
				return (1);
		} else
			if (seq_set(sp, tkp->name, strlen(tkp->name),
			    t, strlen(t), tkp->output, strlen(tkp->output),
			    SEQ_INPUT, SEQ_NOOVERWRITE | SEQ_SCREEN))
				return (1);
	}

	/*
	 * Rework any function key mappings that were set before the
	 * screen was initialized.
	 */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = qp->q.le_next)
		if (F_ISSET(qp, SEQ_FUNCMAP))
			(void)cl_fmap(sp, qp->stype,
			    qp->input, qp->ilen, qp->output, qp->olen);
	return (0);
}

/*
 * cl_term_end --
 *	End the special keys defined by the termcap/terminfo entry.
 *
 * PUBLIC: int cl_term_end __P((SCR *));
 */
int
cl_term_end(sp)
	SCR *sp;
{
	SEQ *qp, *nqp;

	/* Delete screen specific mappings. */
	for (qp = sp->gp->seqq.lh_first; qp != NULL; qp = nqp) {
		nqp = qp->q.le_next;
		if (F_ISSET(qp, SEQ_SCREEN))
			(void)seq_mdel(qp);
	}
	return (0);
}

/*
 * cl_fmap --
 *	Map a function key.
 *
 * PUBLIC: int cl_fmap __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
 */
int
cl_fmap(sp, stype, from, flen, to, tlen)
	SCR *sp;
	seq_t stype;
	CHAR_T *from, *to;
	size_t flen, tlen;
{
	size_t nlen;
	int nf;
	char *p, *t, keyname[64];

	VI_INIT_IGNORE(sp);

	(void)snprintf(keyname, sizeof(keyname), "kf%d", atoi(from + 1));
	if ((t = tigetstr(keyname)) == NULL || t == (char *)-1)
		t = NULL;
	if (t == NULL) {
		p = msg_print(sp, from, &nf);
		msgq(sp, M_ERR, "233|This terminal has no %s key", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}

	nlen = snprintf(keyname,
	    sizeof(keyname), "function key %d", atoi(from + 1));
	return (seq_set(sp, keyname, nlen,
	    t, strlen(t), to, tlen, stype, SEQ_NOOVERWRITE | SEQ_SCREEN));
}

/*
 * cl_optchange --
 *	Curses screen specific "option changed" routine.
 *
 * PUBLIC: int cl_optchange __P((SCR *, int));
 */
int
cl_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	CL_PRIVATE *clp;
	char buf[1024];

	clp = CLP(sp);
	switch (opt) {
	case O_COLUMNS:
		/*
		 * XXX
		 * The actual changing of the row/column and terminal value is
		 * done by putting them into the environment, which is used by
		 * curses.  Stupid, but ugly.
		 *
		 * Set the columns value in the environment for curses.
		 */
		(void)snprintf(buf,
		    sizeof(buf), "COLUMNS=%lu", O_VAL(sp, O_COLUMNS));
		if (cl_putenv(buf))
			return (1);

		VI_INIT_IGNORE(sp);
		F_SET(clp, CL_SIGWINCH);
		break;
	case O_LINES:
		/*
		 * XXX
		 * See comment for O_COLUMNS.
		 *
		 * Set the rows value in the environment for curses.
		 */
		(void)snprintf(buf,
		    sizeof(buf), "LINES=%lu", O_VAL(sp, O_LINES));
		if (cl_putenv(buf))
			return (1);

		VI_INIT_IGNORE(sp);
		F_SET(clp, CL_SIGWINCH);
		break;
	case O_TERM:
		/*
		 * XXX
		 * See comment for O_COLUMNS.
		 *
		 * Set the terminal value in the environment for curses.
		 */
		(void)snprintf(buf, sizeof(buf), "TERM=%s", O_VAL(sp, O_TERM));
		if (cl_putenv(buf))
			return (1);
		
		VI_INIT_IGNORE(sp);

		/* Restart curses.  If this fails, we're done. */
		if (F_ISSET(sp, S_EX)) {
			cl_ex_end(sp);
			if (cl_ex_init(sp)) {
				v_end(sp->gp);
				exit (1);
			}
		} else {
			cl_vi_end(sp);
			if (cl_vi_init(sp)) {
				v_end(sp->gp);
				exit (1);
			}
		}
		F_SET(clp, CL_SIGWINCH);
		break;
	}
	return (0);
}


/*
 * cl_ssize --
 *	Return the terminal size.
 *
 * PUBLIC: int cl_ssize __P((SCR *, int, recno_t *, size_t *));
 */
int
cl_ssize(sp, sigwinch, rowp, colp)
	SCR *sp;
	int sigwinch;
	recno_t *rowp;
	size_t *colp;
{
#ifdef TIOCGWINSZ
	struct winsize win;
#endif
	size_t col, row;
	int nf, rval;
	char *p, *s, buf[2048];

	/*
	 * !!!
	 * sp may be NULL.
	 *
	 * Get the screen rows and columns.  If the values are wrong, it's
	 * not a big deal -- as soon as the user sets them explicitly the
	 * environment will be set and the screen package will use the new
	 * values.
	 *
	 * Try TIOCGWINSZ.
	 */
	row = col = 0;
#ifdef TIOCGWINSZ
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &win) != -1) {
		row = win.ws_row;
		col = win.ws_col;
	}
#endif
	/* If here because of suspend or a signal, only trust TIOCGWINSZ. */
	if (sigwinch) {
		/*
		 * Somebody didn't get TIOCGWINSZ right, or has suspend
		 * without window resizing support.  The user just lost,
		 * but there's nothing we can do.
		 */
		if (row == 0 || col == 0)
			return (1);

		/*
		 * SunOS systems deliver SIGWINCH when windows are uncovered
		 * as well as when they change size.  In addition, we call
		 * here when continuing after being suspended since the window
		 * may have changed size.  Since we don't want to background
		 * all of the screens just because the window was uncovered,
		 * ignore the signal if there's no change.
		 */
		if (sp != NULL &&
		    row == O_VAL(sp, O_LINES) && col == O_VAL(sp, O_COLUMNS))
			return (1);

		if (rowp != NULL)
			*rowp = row;
		if (colp != NULL)
			*colp = col;
		return (0);
	}

	/*
	 * !!!
	 * If TIOCGWINSZ failed, or had entries of 0, try termcap.  This
	 * routine is called before any termcap or terminal information
	 * has been set up.  If there's no TERM environmental variable set,
	 * let it go, at least ex can run.
	 */
	if (row == 0 || col == 0) {
		if ((s = getenv("TERM")) == NULL)
			goto noterm;
		if (row == 0)
			if ((rval = tigetnum("lines")) < 0)
				msgq(sp, M_SYSERR, "tigetnum: lines");
			else
				row = rval;
		if (col == 0)
			if ((rval = tigetnum("cols")) < 0)
				msgq(sp, M_SYSERR, "tigetnum: cols");
			else
				col = rval;
	}

	/* If nothing else, well, it's probably a VT100. */
noterm:	if (row == 0)
		row = 24;
	if (col == 0)
		col = 80;

	/*
	 * !!!
	 * POSIX 1003.2 requires the environment to override everything.
	 * Often, people can get nvi to stop messing up their screen by
	 * deleting the LINES and COLUMNS environment variables from their
	 * dot-files.
	 */
	if ((s = getenv("LINES")) != NULL)
		row = strtol(s, NULL, 10);
	if ((s = getenv("COLUMNS")) != NULL)
		col = strtol(s, NULL, 10);

	if (rowp != NULL)
		*rowp = row;
	if (colp != NULL)
		*colp = col;
	return (0);
}

/*
 * cl_putenv --
 *	Put a value into the environment.  We use putenv(3) because it's
 *	more portable.  The following hack is because some moron decided
 *	to keep a reference to the memory passed to putenv(3), instead of
 *	having it allocate its own.  Someone clearly needs to get promoted
 *	into management.
 */
static int
cl_putenv(s)
	char *s;
{
	/* XXX: Unfixable memory leak. */
	return ((s = strdup(s)) == NULL ? 1 : putenv(s));
}

/*
 * cl_putchar --
 *	Function version of putchar, for tputs.
 *
 * PUBLIC: int cl_putchar __P((int));
 */
int
cl_putchar(ch)
	int ch;
{
	return (putchar(ch));
}
