/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_screen.c,v 8.3 1995/01/24 10:51:42 bostic Exp $ (Berkeley) $Date: 1995/01/24 10:51:42 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "cl.h"
#include "../svi/svi_screen.h"

int
cl_init(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	SVI_PRIVATE *svp;
	struct termios t;
	int nf;
	char *p;

	CALLOC_RET(sp, clp, CL_PRIVATE *, 1, sizeof(CL_PRIVATE));
	CLP(sp) = clp;

#ifdef SYSV_CURSES
	/*
	 * The SunOS/System V initscr() isn't reentrant.  Don't even think
	 * about trying to use it.  It fails in subtle ways (e.g. select(2)
	 * on fileno(stdin) stops working).  We don't care about the SCREEN
	 * reference returned by newterm, we never have more than one SCREEN
	 * at a time.
	 */
	errno = 0;
	if (newterm(O_STR(sp, O_TERM), stdout, stdin) == NULL) {
		if (errno)
			msgq(sp, M_SYSERR, "newterm");
		else
			msgq(sp, M_ERR, "Error: newterm");
		return (1);
	}
#else
	/*
	 * Initscr() doesn't provide useful error values or messages.  The
	 * reasonable guess is that either malloc failed or the terminal was
	 * unknown or lacking some essential feature.  Try and guess so the
	 * user isn't even more pissed off because of the error message.
	 */
	errno = 0;
	if (initscr() == NULL) {
		char kbuf[2048];
		if (errno)
			msgq(sp, M_SYSERR, "initscr");
		else
			msgq(sp, M_ERR, "Error: initscr");
		if ((p = getenv("TERM")) == NULL || !strcmp(p, "unknown"))
			msgq(sp, M_ERR,
	"215|No TERM environment variable set, or TERM set to \"unknown\"");
		else if (tgetent(kbuf, p) != 1) {
			p = msg_print(sp, p, &nf);
			msgq(sp, M_ERR,
"216|%s: unknown terminal type, or terminal lacks necessary features", p);
			if (nf)
				FREE_SPACE(sp, p, 0);
		} else {
			p = msg_print(sp, p, &nf);
			msgq(sp, M_ERR,
		    "217|%s: terminal type lacks necessary features", p);
			if (nf)
				FREE_SPACE(sp, p, 0);
		}
		return (1);
	}
#endif
	/*
	 * We use raw mode.  What we want is 8-bit clean, however, signals
	 * and flow control should continue to work.  Admittedly, it sounds
	 * like cbreak, but it isn't.  Using cbreak() can get you additional
	 * things like IEXTEN, which turns on things like DISCARD and LNEXT.
	 *
	 * !!!
	 * If raw isn't turning off echo and newlines, something's wrong.
	 * However, it doesn't hurt.
	 */
	noecho();			/* No character echo. */
	nonl();				/* No CR/NL translation. */
	raw();				/* 8-bit clean. */
	idlok(stdscr, 1);		/* Use hardware insert/delete line. */

	/*
	 * XXX
	 * Historic implementations of curses handled SIGTSTP signals
	 * in one of three ways.  They either:
	 *
	 *	1: Set their own handler, regardless.
	 *	2: Did not set a handler if a handler was already installed.
	 *	3: Set their own handler, but then called any previously set
	 *	   handler after completing their own cleanup.
	 *
	 * We don't try and figure out which behavior is in place, we
	 * just set it to SIG_DFL after initializing the curses interface.
	 */
	(void)signal(SIGTSTP, SIG_DFL);

	/*
	 * If flow control was on, turn it back on.  Turn signals on.  ISIG
	 * turns on VINTR, VQUIT, VDSUSP and VSUSP.  See signal.c:sig_init()
	 * for a discussion of what's going on here.  To sum up, sig_init()
	 * already installed a handler for VINTR.  We're going to disable the
	 * other three.
	 *
	 * XXX
	 * We want to use ^Y as a vi scrolling command.  If the user has the
	 * DSUSP character set to ^Y (common practice) clean it up.  As it's
	 * equally possible that the user has VDSUSP set to 'a', we disable
	 * it regardless.  It doesn't make much sense to suspend vi at read,
	 * so I don't think anyone will care.  Alternatively, we could look
	 * it up in the table of legal command characters and turn it off if
	 * it matches one.  VDSUSP wasn't in POSIX 1003.1-1990, so we test for
	 * it.
	 *
	 * XXX
	 * We don't check to see if the user had signals enabled to start with.
	 * If they didn't, it's unclear what we're supposed to do here, but it
	 * is also pretty unlikely.
	 */
	if (!tcgetattr(STDIN_FILENO, &t)) {
		if (sp->gp->original_termios.c_iflag & IXON)
			t.c_iflag |= IXON;
		if (sp->gp->original_termios.c_iflag & IXOFF)
			t.c_iflag |= IXOFF;

		t.c_lflag |= ISIG;
#ifdef VDSUSP
		t.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
		t.c_cc[VQUIT] = _POSIX_VDISABLE;
		t.c_cc[VSUSP] = _POSIX_VDISABLE;

		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t);
	}

	/*
	 * The historic 4BSD curses had an uneasy relationship with termcap.
	 * Termcap used a static buffer to hold the terminal information,
	 * which was was then used by the curses functions.  We want to use
	 * it too, for lots of random things, but we've put it off until after
	 * initscr() was called.  Do it now.
	 */
	if (cl_term_init(sp)) {
		free(clp);
		return (1);
	}

	/* Put the cursor keys into application mode. */
	(void)cl_keypad(sp, 1);

	/* Fill in the general functions that the screen owns. */
	sp->e_bell = cl_bell;
	sp->e_fmap = cl_fmap;
	sp->e_suspend = cl_suspend;

	/* Fill in the private functions that the screen owns. */
	svp = SVP(sp);
	svp->scr_addnstr = cl_addnstr;
	svp->scr_addstr = cl_addstr;
	svp->scr_clear = cl_clear;
	svp->scr_clrtoeol = cl_clrtoeol;
	svp->scr_cursor = cl_cursor;
	svp->scr_deleteln = cl_deleteln;
	svp->scr_end = cl_end;
	svp->scr_insertln = cl_insertln;
	svp->scr_inverse = cl_inverse;
	svp->scr_move = cl_move;
	svp->scr_refresh = cl_refresh;
	svp->scr_restore = cl_restore;
	svp->scr_size = cl_ssize;
	svp->scr_winch = cl_winch;

	F_SET(clp, CL_CURSES_INIT);
	return (0);
}

int
cl_end(sp)
	SCR *sp;
{
	/* Restore the cursor keys to normal mode. */
	(void)cl_keypad(sp, 0);

	/* Restore the terminal. */
	cl_term_end(sp);

	/* Move to the bottom of the screen. */
	if (move(sp->t_maxrows, 0) == OK) {		/* XXX */
		clrtoeol();
		refresh();
	}

	/* End curses window. */
	errno = 0;
	if (endwin() == ERR) {
		if (errno)
			msgq(sp, M_SYSERR, "endwin");
		else
			msgq(sp, M_ERR, "Error: endwin");
		return (1);
	}
	return (0);
}

/*
 * cl_copy --
 *	Copy to a new screen.
 */
int
cl_copy(orig, sp)
	SCR *orig, *sp;
{
	CL_PRIVATE *oclp, *nclp;

	/* Create the private screen structure. */
	CALLOC_RET(orig, nclp, CL_PRIVATE *, 1, sizeof(CL_PRIVATE));
	sp->cl_private = nclp;

/* INITIALIZED AT SCREEN CREATE. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig == NULL) {
	} else {
		oclp = CLP(orig);

		if (oclp->VB != NULL && (nclp->VB = strdup(oclp->VB)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}

		F_SET(nclp, F_ISSET(oclp, CL_CURSES_INIT));
	}
	return (0);
}

/*
 * cl_keypad --
 *	Put the keypad/cursor arrows into or out of application mode.
 */
int
cl_keypad(sp, on)
	SCR *sp;
	int on;
{
#ifdef SYSV_CURSES
	keypad(stdscr, on ? TRUE : FALSE);
#else
	char *sbp, *t, sbuf[128];

	sbp = sbuf;
	if ((t = tgetstr(on ? "ks" : "ke", &sbp)) != NULL) {
		(void)tputs(t, 0, vi_putchar);
		(void)fflush(stdout);
	}
#endif
	return (0);
}
