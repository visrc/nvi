/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_screen.c,v 10.19 1995/10/31 10:52:14 bostic Exp $ (Berkeley) $Date: 1995/10/31 10:52:14 $";
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

#include "../common/common.h"
#include "cl.h"

static int cl_ex_end __P((GS *));
static int cl_ex_init __P((SCR *));
static int cl_vi_end __P((GS *));
static int cl_vi_init __P((SCR *));

/*
 * cl_screen --
 *	Switch screen types.
 *
 * PUBLIC: int cl_screen __P((SCR *, u_int32_t));
 */
int
cl_screen(sp, flags)
	SCR *sp;
	u_int32_t flags;
{
	CL_PRIVATE *clp;
	int need_nl;

	clp = CLP(sp);
	need_nl = 0;

	/* See if we're already in the right mode. */
	if (LF_ISSET(S_EX) && F_ISSET(clp, CL_SCR_EX) ||
	    LF_ISSET(S_VI) && F_ISSET(clp, CL_SCR_VI))
		return (0);

	/* Turn off the screen-ready bit. */
	F_CLR(sp, S_SCREEN_READY);

	/*
	 * Fake leaving ex mode.
	 *
	 * We don't actually exit ex or vi mode unless forced (e.g. by a window
	 * size change).  This is because many curses implementations can't be
	 * called twice in a single program.  Plus, it's faster.  If the editor
	 * "leaves" vi to enter ex, when it exits ex we'll just fall back into
	 * vi.
	 */
	if (F_ISSET(clp, CL_SCR_EX)) {
		F_CLR(sp, S_EX);
		F_CLR(clp, CL_SCR_EX);
	}

	/*
	 * Fake leaving vi mode.
	 *
	 * All screens should move to the bottom of the screen, even in the
	 * presence of split screens.  This makes terminal scrolling happen
	 * naturally and without overwriting editor text.  Don't clear the
	 * info line, its contents may be valid, e.g. :file|append.
	 */
	if (F_ISSET(clp, CL_SCR_VI)) {
		(void)move(LINES - 1, 0);
		(void)refresh();
		need_nl = 1;

		F_CLR(sp, S_VI);
		F_CLR(clp, CL_SCR_VI);
	}

	/* Enter the requested mode. */
	if (LF_ISSET(S_EX)) {
		if (cl_ex_init(sp))
			return (1);
		F_SET(sp, S_EX);

		if (need_nl)
			(void)write(STDOUT_FILENO, "\n", 1);
	} else {
		if (cl_vi_init(sp))
			return (1);
		clearok(curscr, 1);
		F_SET(sp, S_VI);
	}
	return (0);
}

/*
 * cl_quit --
 *	Shutdown the screens.
 *
 * PUBLIC: int cl_quit __P((GS *));
 */
int
cl_quit(gp)
	GS *gp;
{
	CL_PRIVATE *clp;
	int rval;

	rval = 0;
	clp = GCLP(gp);

	/* Clean up the terminal mappings. */
	if (cl_term_end(gp))
		rval = 1;

	/*
	 * Really leave ex mode.
	 *
	 * XXX
	 * This call may discard characters from the tty queue.
	 */
	if (F_ISSET(clp, CL_SCR_EX_INIT) && cl_ex_end(gp))
		rval = 1;
	F_CLR(clp, CL_SCR_EX | CL_SCR_EX_INIT);

	/*
	 * Really leave vi mode.
	 *
	 * XXX
	 * This call may discard characters from the tty queue.
	 */
	if (F_ISSET(clp, CL_SCR_VI_INIT) && cl_vi_end(gp))
		rval = 1;
	F_CLR(clp, CL_SCR_VI | CL_SCR_VI_INIT);

	return (rval);
}

/*
 * cl_vi_init --
 *	Initialize the curses vi screen.
 */
static int
cl_vi_init(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	GS *gp;
	char *ttype;

	gp = sp->gp;
	clp = CLP(sp);

	/* If already initialized, just set the terminal modes. */
	if (F_ISSET(clp, CL_SCR_VI_INIT))
		goto fast;

	/* Curses vi always reads from (and writes to) a terminal. */
	if (!F_ISSET(gp, G_STDIN_TTY) || !isatty(STDOUT_FILENO)) {
		msgq(sp, M_ERR,
		    "016|Vi's standard input and output must be a terminal");
		return (1);
	}

	/*
	 * We don't care about the SCREEN reference returned by newterm, we
	 * never have more than one SCREEN at a time.
	 *
	 * XXX
	 * The SunOS initscr() isn't reentrant.  Don't even think about using
	 * it.  It fails in subtle ways (e.g. select(2) on fileno(stdin) stops
	 * working).
	 *
	 * XXX
	 * The HP/UX newterm doesn't support the NULL first argument, so we
	 * have to specify the terminal type.
	 */
	errno = 0;
	ttype = O_STR(sp, O_TERM);
	if (newterm(ttype, stdout, stdin) == NULL) {
		if (errno)
			msgq(sp, M_SYSERR, "%s", ttype);
		else
			msgq(sp, M_ERR, "%s: unknown terminal type", ttype);
		return (1);
	}

	/*
	 * We use raw mode.  What we want is 8-bit clean, however, signals
	 * and flow control should continue to work.  Admittedly, it sounds
	 * like cbreak, but it isn't.  Using cbreak() can get you additional
	 * things like IEXTEN, which turns on flags like DISCARD and LNEXT.
	 *
	 * !!!
	 * If raw isn't turning off echo and newlines, something's wrong.
	 * However, it shouldn't hurt.
	 */
	noecho();			/* No character echo. */
	nonl();				/* No CR/NL translation. */
	raw();				/* 8-bit clean. */
	idlok(stdscr, 1);		/* Use hardware insert/delete line. */

#ifndef TRUE
#define	TRUE	1
#endif
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
	 * that curses isn't going to take the signal.  Since curses isn't
	 * reentrant (i.e., the whole curses SIGTSTP interface is a fantasy),
	 * we're doing The Right Thing.
	 */
	(void)signal(SIGTSTP, SIG_DFL);

	/*
	 * If flow control was on, turn it back on.  Turn signals on.  ISIG
	 * turns on VINTR, VQUIT, VDSUSP and VSUSP.   The main curses code
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
	if (tcgetattr(STDIN_FILENO, &clp->vi_enter)) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err;
	}
	if (clp->orig.c_iflag & IXON)
		clp->vi_enter.c_iflag |= IXON;
	if (clp->orig.c_iflag & IXOFF)
		clp->vi_enter.c_iflag |= IXOFF;

	clp->vi_enter.c_lflag |= ISIG;
#ifdef VDSUSP
	clp->vi_enter.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
	clp->vi_enter.c_cc[VQUIT] = _POSIX_VDISABLE;
	clp->vi_enter.c_cc[VSUSP] = _POSIX_VDISABLE;

	/* Initialize terminal based information. */
	if (cl_term_init(sp))
		goto err;

fast:	if (tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &clp->vi_enter)) {
		msgq(sp, M_SYSERR, "tcsetattr");
err:		(void)cl_vi_end(sp->gp);
		return (1);
	}


#ifdef NCURSES_DEBUG
	trace(TRACE_UPDATE | TRACE_MOVE | TRACE_CALLS);
#endif

	F_SET(clp, CL_SCR_VI | CL_SCR_VI_INIT);
	return (0);
}

/*
 * cl_vi_end --
 *	Shutdown the vi screen.
 */
static int
cl_vi_end(gp)
	GS *gp;
{
	CL_PRIVATE *clp;

	clp = GCLP(gp);
	if (!F_ISSET(clp, CL_SCR_VI_INIT))
		return (0);
	F_CLR(clp, CL_SCR_VI_INIT);
	if (!F_ISSET(gp, G_STDIN_TTY))
		return (0);

#ifndef FALSE
#define	FALSE	0
#endif
	/* Restore the cursor keys to normal mode. */
	(void)keypad(stdscr, FALSE);

	/*
	 * Move to the bottom of the window, clear that line.  Some
	 * implementations of endwin() don't do this for you.
	 */
	(void)move(LINES - 1, 0);
	(void)clrtoeol();
	(void)refresh();

	/* End curses window. */
	(void)endwin();

	return (0);
}

/*
 * cl_ex_init --
 *	Initialize the ex screen.
 */
static int
cl_ex_init(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	size_t len;
	char *t;

	clp = CLP(sp);

	/* If already initialized, just set the terminal modes. */
	if (F_ISSET(clp, CL_SCR_EX_INIT))
		goto fast;

	/* If not reading from a file, we're done. */
	if (!F_ISSET(sp->gp, G_STDIN_TTY))
		return (0);

	/* Get the ex termcap/terminfo strings. */
#define	GETCAP(name, element) {						\
	if ((t = tigetstr(name)) != NULL &&				\
	    t != (char *)-1 && (len = strlen(t)) != 0) {		\
		MALLOC_RET(sp, clp->element, char *, len + 1);		\
		memmove(clp->element, t, len + 1);			\
	}								\
}
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
	clp->ex_enter = clp->orig;
	clp->ex_enter.c_lflag  |= ECHO | ECHOE | ECHOK | ICANON | IEXTEN | ISIG;
#ifdef ECHOCTL
	clp->ex_enter.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
	clp->ex_enter.c_lflag |= ECHOKE;
#endif
	clp->ex_enter.c_iflag |= ICRNL;
	clp->ex_enter.c_oflag |= OPOST;
#ifdef ONLCR
	clp->ex_enter.c_oflag |= ONLCR;
#endif

fast:	if (tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &clp->ex_enter)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		return (1);
	}

	/* Move to the last line on the screen. */
	if (clp->cup != NULL)
		tputs(tgoto(clp->cup, 0, LINES - 1), 1, cl_putchar);

	F_SET(clp, CL_SCR_EX | CL_SCR_EX_INIT);
	return (0);
}

/*
 * cl_ex_end --
 *	Shutdown the ex screen.
 */
static int
cl_ex_end(gp)
	GS *gp;
{
	CL_PRIVATE *clp;

	clp = GCLP(gp);
	if (!F_ISSET(clp, CL_SCR_EX_INIT))
		return (0);
	F_CLR(clp, CL_SCR_EX_INIT);
	if (!F_ISSET(gp, G_STDIN_TTY))
		return (0);

	/* Restore the original terminal modes. */
	(void)tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &clp->orig);
	F_CLR(clp, CL_SCR_EX_INIT);

#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
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
#endif
	return (0);
}
