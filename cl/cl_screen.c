/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_screen.c,v 10.7 1995/06/22 19:22:22 bostic Exp $ (Berkeley) $Date: 1995/06/22 19:22:22 $";
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

#include "common.h"
#include "cl.h"

static int	cl_common __P((SCR *));

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
	int nf;
	char *p;

	/* Curses vi always reads from (and writes to) a terminal. */
	gp = sp->gp;
	if (!F_ISSET(gp, G_STDIN_TTY) || !isatty(STDOUT_FILENO)) {
		msgq(sp, M_ERR,
		    "016|Vi's standard input and output must be a terminal");
		return (1);
	}

	if (gp->cl_private == NULL && cl_common(sp))
		return (1);

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
	"229|No TERM environment variable set, or TERM set to \"unknown\"");
		else if (tgetent(kbuf, p) != 1) {
			p = msg_print(sp, p, &nf);
			msgq(sp, M_ERR,
"230|%s: unknown terminal type, or terminal lacks necessary features", p);
			if (nf)
				FREE_SPACE(sp, p, 0);
		} else {
			p = msg_print(sp, p, &nf);
			msgq(sp, M_ERR,
		    "231|%s: terminal type lacks necessary features", p);
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
	 * However, it shouldn't hurt.
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
	 * set it to SIG_DFL after initializing the curses interface.
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

	/* Things are now initialized -- set the bit. */
	F_SET(CLP(sp), CL_INIT_VI);

	/*
	 * The historic 4BSD curses had an uneasy relationship with termcap.
	 * Termcap used a static buffer to hold the terminal information,
	 * which was was then used by the curses functions.  We want to use
	 * it too, for lots of random things, but we've put it off until after
	 * initscr() was called and the CL_INIT_VI bit was set.  Do it now.
	 */
	if (cl_term_init(sp)) {
err:		(void)cl_vi_end(sp);
		return (1);
	}

	/* Put the cursor keys into application mode. */
	(void)cl_keypad(1);

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

	clp = CLP(sp);
	if (!F_ISSET(CLP(sp), CL_INIT_VI))
		return (0);

	/* Restore signals. */
	cl_sig_end(sp);

	/* Restore the cursor keys to normal mode. */
	(void)cl_keypad(0);

	/* Restore the terminal, and map sequences. */
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
	size_t len;
	char *s, *t, buf[128], tbuf[2048];

	clp = CLP(sp);
	if (clp == NULL) {
		if (cl_common(sp))
			return (1);
		clp = CLP(sp);
	}

	/* Get the termcap entry. */
	if (tgetent(tbuf, O_STR(sp, O_TERM)) != 1)
		return (1);

	/* Get CE. */
	t = buf;
	if ((t = tgetstr("ce", &t)) != NULL && (len = strlen(t)) != 0) {
		MALLOC_NOMSG(sp, s, char *, len + 1);
		if (s != NULL) {
			memmove(s, buf, len);
			s[len] = '\0';
			clp->CE = s;
		}
	}

	/* Get CM. */
	t = buf;
	if ((t = tgetstr("cm", &t)) != NULL && (len = strlen(t)) != 0) {
		MALLOC_NOMSG(sp, s, char *, len + 1);
		if (s != NULL) {
			memmove(s, buf, len);
			s[len] = '\0';
			clp->CM = s;
		}
	}

	/* Get SE. */
	t = buf;
	if ((t = tgetstr("se", &t)) != NULL && (len = strlen(t)) != 0) {
		MALLOC_NOMSG(sp, s, char *, len + 1);
		if (s != NULL) {
			memmove(s, buf, len);
			s[len] = '\0';
			clp->SE = s;
		}
	}

	/* Get SO. */
	t = buf;
	if ((t = tgetstr("so", &t)) != NULL && (len = strlen(t)) != 0) {
		MALLOC_NOMSG(sp, s, char *, len + 1);
		if (s != NULL) {
			memmove(s, buf, len);
			s[len] = '\0';
			clp->SO = s;
		}
	}

	/* SE and SO are paired. */
	if (clp->SE == NULL || clp->SO == NULL) {
		if (clp->SE != NULL) {
			free(clp->SE);
			clp->SE = NULL;
		}
		if (clp->SO != NULL) {
			free(clp->SO);
			clp->SO = NULL;
		}
	}

	/* Get UP. */
	t = buf;
	if ((t = tgetstr("up", &t)) != NULL && (len = strlen(t)) != 0) {
		MALLOC_NOMSG(sp, s, char *, len + 1);
		if (s != NULL) {
			memmove(s, buf, len);
			s[len] = '\0';
			clp->UP = s;
		}
	}

	/* Initialize the tty. */
	if (cl_ex_tinit(sp))
		return (1);

	/* Things are now initialized -- set the bit. */
	F_SET(clp, CL_INIT_EX);

	/* Move to the last line on the screen. */
	if (clp->UP != NULL && clp->CM != NULL)
		tputs(tgoto(clp->CM, 0, O_VAL(sp, O_LINES) - 1), 1, cl_putchar);

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

	clp = CLP(sp);
	if (!F_ISSET(CLP(sp), CL_INIT_EX))
		return (0);

	/* Restore signals. */
	cl_sig_end(sp);

	/* Restore the tty. */
	if (cl_ex_tend(sp))
		return (1);

#if defined(DEBUG) || defined(PURIFY) || !defined(STANDALONE)
	clp = CLP(sp);
	if (clp->CE != NULL)
		free(clp->CE);
	if (clp->CM != NULL)
		free(clp->CM);
	if (clp->UP != NULL)
		free(clp->UP);
	if (clp->SE != NULL)
		free(clp->SE);
	if (clp->SO != NULL)
		free(clp->SO);

	if (clp->lline != NULL)
		free(clp->lline);
	free(clp);
#endif
	sp->gp->cl_private = NULL;

	return (0);
}

/*
 * cl_ex_tinit --
 *	Enter ex terminal mode.  Separated out because it's used by the vi
 *	code to run ex commands that don't run through vi.
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
 *	Leave ex terminal mode.  Separated out because it's used by the vi
 *	code to run ex commands that don't run through vi.
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

	clp = CLP(sp);
	if (tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &clp->exterm)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		return (1);
	}

	F_CLR(sp, S_EX_CANON);
	return (0);
}

/*
 * cl_common --
 *	Allocation and common initialization for the CL private structure.
 */
static int
cl_common(sp)
	SCR *sp;
{
	GS *gp;
	CL_PRIVATE *clp;

	gp = sp->gp;

	CALLOC_RET(sp, clp, CL_PRIVATE *, 1, sizeof(CL_PRIVATE));
	gp->cl_private = clp;

	/* Start catching signals. */
	if (cl_sig_init(sp))
		return (1);

	/* Initialize the functions. */
	gp->scr_addnstr = cl_addnstr;
	gp->scr_addstr = cl_addstr;
	gp->scr_attr = cl_attr;
	gp->scr_bell = cl_bell;
	gp->scr_busy = cl_busy;
	gp->scr_canon = cl_canon;
	gp->scr_clrtoeol = cl_clrtoeol;
	gp->scr_cursor = cl_cursor;
	gp->scr_deleteln = cl_deleteln;
	gp->scr_discard = cl_discard;
	gp->scr_ex_adjust = cl_ex_adjust;
	gp->scr_ex_init = cl_ex_init;
	gp->scr_fmap = cl_fmap;
	gp->scr_getkey = cl_getkey;
	gp->scr_insertln = cl_insertln;
	gp->scr_interrupt = cl_interrupt;
	gp->scr_move = cl_move;
	gp->scr_refresh = cl_refresh;
	gp->scr_resize = cl_resize;
	gp->scr_split = cl_split;
	gp->scr_suspend = cl_suspend;

	return (0);
}
