/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_screen.c,v 10.11 1995/07/06 11:53:39 bostic Exp $ (Berkeley) $Date: 1995/07/06 11:53:39 $";
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
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "common.h"
#include "cl.h"

/*
 * cl_vi_init --
 *	Initialize the vi screen.
 *
 * PUBLIC: int cl_vi_init __P((SCR *));
 */
int
cl_vi_init(sp)
	SCR *sp;
{
	GS *gp;
	struct termios t;

	/*
	 * This function initializes the curses library for vi.
	 *
	 * None of this will be useful for any other screen, with the
	 * possible exception of as a laundry list of things that are
	 * worth thinking about.
	 *
	 * Curses vi always reads from (and writes to) a terminal.
	 */
	gp = sp->gp;
	if (!F_ISSET(gp, G_STDIN_TTY) || !isatty(STDOUT_FILENO)) {
		msgq(sp, M_ERR,
		    "016|Vi's standard input and output must be a terminal");
		return (1);
	}

	/*
	 * The SunOS/System V initscr() isn't reentrant.  Don't even think
	 * about trying to use it.  It fails in subtle ways (e.g. select(2)
	 * on fileno(stdin) stops working).  We don't care about the SCREEN
	 * reference returned by newterm, we never have more than one SCREEN
	 * at a time.
	 */
	errno = 0;
	if (newterm(NULL, stdout, stdin) == NULL) {
		if (errno)
			msgq(sp, M_SYSERR, "%s", O_STR(sp, O_TERM));
		else
			msgq(sp, M_ERR,
			    "%s: unknown terminal type", O_STR(sp, O_TERM));
		return (1);
	}

	/*
	 * We use raw mode.  What we want is 8-bit clean, however, signals
	 * and flow control should continue to work.  Admittedly, it sounds
	 * like cbreak, but it isn't.  Using cbreak() can get you additional
	 * things like IEXTEN, which turns on things like DISCARD and LNEXT.
	 *
	 * !!!
	 * If raw isn't turning off echo and newlines, something's wrong.
	 * However, it shouldn't hurt.
	 */
	noecho();			/* No character echo. */
	nonl();				/* No CR/NL translation. */
	raw();				/* 8-bit clean. */
	idlok(stdscr, 1);		/* Use hardware insert/delete line. */

	/* Put the cursor keys into application mode. */
	(void)keypad(stdscr, TRUE);

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
	 * We don't try and figure out which behavior is in place, we force
	 * it to SIG_DFL after initializing the curses interface, which means
	 * that curses isn't going to take the signal.
	 */
	(void)signal(SIGTSTP, SIG_DFL);

	/*
	 * If flow control was on, turn it back on.  Turn signals on.  ISIG
	 * turns on VINTR, VQUIT, VDSUSP and VSUSP.  See cl_sig_init() for
	 * a discussion of what's going on here.  To sum up, cl_sig_init()
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
	 * We don't check to see if the user had signals enabled originally.
	 * If they didn't, it's unclear what we're supposed to do here, but
	 * it's also pretty unlikely.
	 */
	if (tcgetattr(STDIN_FILENO, &t)) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err;
	}
	if (gp->original_termios.c_iflag & IXON)
		t.c_iflag |= IXON;
	if (gp->original_termios.c_iflag & IXOFF)
		t.c_iflag |= IXOFF;

	t.c_lflag |= ISIG;
#ifdef VDSUSP
	t.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
	t.c_cc[VQUIT] = _POSIX_VDISABLE;
	t.c_cc[VSUSP] = _POSIX_VDISABLE;

	if (tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		goto err;
	}

	/* Initialize terminal based information. */
	if (cl_term_init(sp)) {
err:		(void)cl_vi_end(sp);
		return (1);
	}

	F_SET(CLP(sp), CL_INIT_VI);
	return (0);
}

/*
 * cl_vi_end --
 *	Shutdown the vi screen.
 *
 * PUBLIC: int cl_vi_end __P((SCR *));
 */
int
cl_vi_end(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;

	/*
	 * This function ends the curses library for vi.
	 *
	 * None of this will be useful for any other screen, with the
	 * possible exception of as a laundry list of things that are
	 * worth thinking about.
	 */
	clp = CLP(sp);
	if (!F_ISSET(clp, CL_INIT_VI))
		return (0);

	/* Restore the cursor keys to normal mode. */
	(void)keypad(stdscr, FALSE);

	/* End curses window. */
	(void)endwin();

	/* Restore the terminal, and map sequences. */
	cl_term_end(sp);

#if defined(DEBUG) || defined(PURIFY) || !defined(STANDALONE)
	if (clp->lline != NULL)
		free(clp->lline);
	free(clp);
#endif
	sp->gp->cl_private = NULL;
	return (0);
}

/*
 * cl_ex_init --
 *	Initialize the ex screen.
 *
 * PUBLIC: int cl_ex_init __P((SCR *));
 */
int
cl_ex_init(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	int err;
	char *t;

	/*
	 * This function initializes the curses library for ex.
	 *
	 * None of this will be useful for any other screen, with the
	 * possible exception of as a laundry list of things that are
	 * worth thinking about.
	 */

#define	GETCAP(name, element) {						\
	size_t __len;							\
	if ((t = tigetstr(name)) != NULL &&				\
	    t != (char *)-1 && (__len = strlen(t)) != 0) {		\
		MALLOC_RET(sp, clp->element, char *, __len + 1);	\
		memmove(clp->element, t, __len + 1);			\
	}								\
}
	/*
	 * Set up the terminal database information, and get cursor_address,
	 * enter_standout_mode, exit_standout_mode, cursor_up, clr_eol.
	 */
	clp = CLP(sp);
	setupterm(O_STR(sp, O_TERM), STDOUT_FILENO, &err);
	switch (err) {
	case -1:
		msgq(sp, M_ERR, "No terminal database found");
		break;
	case 0:
		msgq(sp, M_ERR, "%s: unknown terminal type", O_STR(sp, O_TERM));
		break;
	case 1:
		GETCAP("cup", cup);
		GETCAP("smso", smso);
		GETCAP("rmso", rmso);
		GETCAP("el", el);
		GETCAP("cuu1", cuu1);

		/* Enter_standout_mode and exit_standout_mode are paired. */
		if (clp->smso == NULL || clp->rmso == NULL) {
			if (clp->smso != NULL) {
				free(clp->smso);
				clp->smso = NULL;
			}
			if (clp->rmso != NULL) {
				free(clp->rmso);
				clp->rmso = NULL;
			}
		}
		break;
	}

	/* Initialize the tty. */
	if (cl_ex_tinit(sp))
		return (1);

	/* Move to the last line on the screen. */
	if (clp->cup != NULL)
		tputs(tgoto(clp->cup,
		    0, O_VAL(sp, O_LINES) - 1), 1, cl_putchar);

	F_SET(clp, CL_INIT_EX);
	return (0);
}

/*
 * cl_ex_end --
 *	Shutdown the ex screen.
 *
 * PUBLIC: int cl_ex_end __P((SCR *));
 */
int
cl_ex_end(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;

	/*
	 * This function ends the curses library for ex.
	 *
	 * None of this will be useful for any other screen, with the
	 * possible exception of as a laundry list of things that are
	 * worth thinking about.
	 */

	clp = CLP(sp);
	if (!F_ISSET(clp, CL_INIT_EX))
		return (0);

	/* Restore the tty. */
	if (cl_ex_tend(sp))
		return (1);

#if defined(DEBUG) || defined(PURIFY) || !defined(STANDALONE)
	clp = CLP(sp);
	if (clp->el != NULL)
		free(clp->el);
	if (clp->cup != NULL)
		free(clp->cup);
	if (clp->cuu1 != NULL)
		free(clp->cuu1);
	if (clp->rmso != NULL)
		free(clp->rmso);
	if (clp->smso != NULL)
		free(clp->smso);

	if (clp->lline != NULL)
		free(clp->lline);
#endif
	F_CLR(clp, CL_INIT_EX);
	return (0);
}

/*
 * cl_ex_tinit --
 *	Enter ex terminal mode.
 *
 * PUBLIC: int cl_ex_tinit __P((SCR *));
 */
int
cl_ex_tinit(sp)
	SCR *sp;
{
	struct termios term;
	CL_PRIVATE *clp;

	if (!F_ISSET(sp->gp, G_STDIN_TTY))
		return (0);

	/*
	 * This routine is used by vi (see cl_canon) to temporarily switch
	 * into ex canonical mode.  Examples are commands like :append, or
	 * "r ! cat /dev/tty", which require a normal UNIX terminal interface
	 * to run.  There are only a few of these commands, and they can
	 * probably be discarded without too much annoyance by your users.
	 * Returning 1 will cause the editor to print a message and fail
	 * gracefully.
	 */

	/* Save the current settings. */
	clp = CLP(sp);
	if (tcgetattr(STDIN_FILENO, &clp->exterm)) {
		msgq(sp, M_SYSERR, "tcgetattr");
		return (1);
	}

	/*
	 * Turn on canonical mode, with normal input and output processing.
	 * Start with the original terminal settings as the user probably
	 * had them (including any local extensions) set correctly for the
	 * current terminal.
	 *
	 * !!!
	 * We can't get everything that we need portably; for example, ONLCR,
	 * mapping <newline> to <carriage-return> on output isn't required
	 * by POSIX 1003.1b-1993.  If this turns out to be a problem, then
	 * we'll either have to play some games on the mapping, or we'll have
	 * to make all ex printf's output \r\n instead of \n.
	 */
	term = sp->gp->original_termios;
	term.c_lflag  |= ECHO | ECHOE | ECHOK | ICANON | IEXTEN | ISIG;
#ifdef ECHOCTL
	term.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
	term.c_lflag |= ECHOKE;
#endif
	term.c_iflag |= ICRNL;
	term.c_oflag |= OPOST;
#ifdef ONLCR
	term.c_oflag |= ONLCR;
#endif
	if (tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &term)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		return (1);
	}

	F_SET(sp, S_EX_CANON);
	return (0);
}

/*
 * cl_ex_tend --
 *	Leave ex terminal mode.
 *
 * PUBLIC: int cl_ex_tend __P((SCR *));
 */
int
cl_ex_tend(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;

	if (!F_ISSET(sp->gp, G_STDIN_TTY))
		return (0);

	/* See the comment in cl_ex_tinit. */
	clp = CLP(sp);
	if (tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &clp->exterm)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		return (1);
	}

	F_CLR(sp, S_EX_CANON);
	return (0);
}
