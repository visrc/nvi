/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_main.c,v 10.9 1995/07/26 12:15:47 bostic Exp $ (Berkeley) $Date: 1995/07/26 12:15:47 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "common.h"
#include "cl.h"

GS *__global_list;			/* GLOBAL: List of screens. */

static int	 cl_sig_init __P((SCR *));
static void	 cl_sig_end __P((GS *));
static GS	*gs_init __P((char *));
static void	 killsig __P((SCR *));

/*
 * main --
 *	This is the main loop for the standalone curses editor.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	static int reenter;
	struct timeval t, *tp;
	CHAR_T ch;
	CL_PRIVATE *clp;
	EVENT ev;
	GS *gp;
	MSG *mp, *freep;
	SCR *next, *sp;
	recno_t rows;
	size_t cols, len;
	u_int32_t flags, timeout;
	int killset, nr, rval;
	const char *p;

	/* If loaded at 0 and jumping through a NULL pointer, stop. */
	if (reenter++)
		abort();

	/* Create and initialize the global structure. */
	__global_list = gp = gs_init(argv[0]);

	/* Figure out how big the screen is. */
	if (cl_ssize(NULL, 0, &rows, &cols))
		exit (1);

	/* Create first screen structure, parse the arguments. */
	switch (v_init(gp, argc, argv, rows, cols)) {
	case INIT_OK:
		break;
	case INIT_DONE:
		exit (0);
	case INIT_ERR:
		exit (1);
	case INIT_USAGE:
		(void)fprintf(stderr,
	"usage: ex [-eFRrsv] [-c command] [-t tag] [-w size] [files ...]\n");
		(void)fprintf(stderr,
	"usage: vi [-eFlRrv] [-c command] [-t tag] [-w size] [files ...]\n");
		exit (1);
	}

	/* Edit the first screen. */
	sp = gp->dq.cqh_first;

	/*
	 * !!!
	 * There's this little chicken and egg problem.  The screen isn't
	 * initialized until all of the exrc files have been read and the
	 * first file loaded.  Unfortunately, you can put any ex command
	 * you want into an exrc file, or as a command-line command.  There
	 * are two categories of problem.
	 *
	 * First, if something required ex to write to the screen, we'll
	 * want to wait before overwriting it.  Second, if ex had to do
	 * serious processing, then the terminal will have been set up.
	 * So, if the screen has been initialized for ex when we get here,
	 * clean up.
	 */
	clp = CLP(sp);
	if (F_ISSET(sp, S_VI)) {
		if (F_ISSET(clp, CL_INIT_EX)) {
			p = msg_cmsg(sp, CMSG_CONT, &len);
			(void)write(STDOUT_FILENO, p, len);
			if (cl_getkey(sp, &ch))
				goto err;

			/*
			 * XXX
			 * This call may discard characters from the tty queue.
			 */
			if (cl_ex_end(sp))
				goto err;
		}
		if (cl_vi_init(sp))
			goto err;
	} else
		if (!F_ISSET(clp, CL_INIT_EX) && cl_ex_init(sp))
			goto err;

	/* Start catching signals. */
	if (cl_sig_init(sp))
		return (1);

	/* Editor startup. */
	timeout = 0;
	ev.e_event = E_START;
	if (v_event_handler(sp, &ev, &timeout))
		goto err;

	/*
	 * Messages get written before we have a screen on which to put them.
	 * Make sure they get displayed as soon as we can.
	 */
	for (mp = gp->msgq.lh_first; mp != NULL;) {
		(void)cl_msg(sp, mp->mtype, mp->buf, mp->len);
		freep = mp;
		mp = mp->q.le_next;
		LIST_REMOVE(freep, q);
		free(freep->buf);
		free(freep);
	}

#ifdef NCURSES_DEBUG
	trace(TRACE_UPDATE | TRACE_MOVE | TRACE_CALLS);
#endif
	for (killset = 0;;) {				/* Edit loop. */
		for (;;) {				/* Event loop. */
			/*
			 * Queue signal based events.  Deliver SIGCONT before
			 * SIGWINCH, so editor can terminate actions before
			 * dealing with the screen size changing.
			 */
			if (F_ISSET(clp, CL_SIGCONT |
			    CL_SIGHUP | CL_SIGINT | CL_SIGTERM | CL_SIGWINCH)) {
				if (F_ISSET(clp, CL_SIGCONT)) {
					F_CLR(clp, CL_SIGCONT);
					ev.e_event = E_SIGCONT;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGHUP)) {
					killset = 1;
					ev.e_event = E_SIGHUP;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGINT)) {
					F_CLR(clp, CL_SIGINT);
					ev.e_event = E_INTERRUPT;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGTERM)) {
					killset = 1;
					ev.e_event = E_SIGTERM;
					goto handle;
				}
				if (F_ISSET(clp, CL_SIGWINCH)) {
					F_CLR(clp, CL_SIGWINCH);
					if (!cl_ssize(sp, 1,
					    &ev.e_lno, &ev.e_cno)) {
						ev.e_event = E_RESIZE;
						goto handle;
					}
				}
			}

			/* Characters may have been queued. */
			if (clp->icnt != 0) {
				nr = clp->icnt;
				clp->icnt = 0;
				goto charq;
			}

			/*
			 * If the upper layer is timing out, set it up.
			 */
			if (timeout != 0) {
				t.tv_sec = timeout / 1000;
				t.tv_usec = timeout % 1000;
				tp = &t;
			} else
				tp = NULL;

			/* Read input characters. */
			switch (cl_read(sp,
			    clp->ibuf, sizeof(clp->ibuf), &nr, tp)) {
			case INP_OK:
charq:				ev.e_csp = clp->ibuf;
				ev.e_len = nr;
				ev.e_event = E_STRING;
				break;
			case INP_EOF:
				ev.e_event = E_EOF;
				break;
			case INP_ERR:
				ev.e_event = E_ERR;
				break;
			case INP_INTR:
				ev.e_event = EINTR;
				break;
			case INP_TIMEOUT:
				ev.e_event = E_TIMEOUT;
				break;
			}

			/*
			 * Send events until the screen exits or the editor
			 * mode changes.
			 */
handle:			timeout = 0;
			flags =
			    F_ISSET(sp, S_EX | S_VI | S_EXIT | S_EXIT_FORCE);
			if (v_event_handler(sp, &ev, &timeout))
				goto err;
			if (killset || flags !=
			    F_ISSET(sp, S_EX | S_VI | S_EXIT | S_EXIT_FORCE))
				break;

			/*
			 * Check for switched screens, flush any waiting
			 * messages.
			 */
			if (F_ISSET(sp, S_SSWITCH)) {
				F_CLR(sp, S_SSWITCH);

				cl_resolve(sp, 0);
				sp = sp->nextdisp;
				cl_resolve(sp, 1);
			}
		}

		/* If a killer signal arrived, we're done. */
		if (killset) {
			killsig(sp);
			/* NOTREACHED */
		}

		/*
		 * Edit mode change.  Shutdown the current editor and
		 * start the new one.
		 */
		if (F_ISSET(sp, S_EX | S_VI) != LF_ISSET(S_EX | S_VI)) {
			ev.e_event = E_STOP;
			if (v_event_handler(sp, &ev, &timeout))
				goto err;

			if (F_ISSET(sp, S_EX)) {
				if (cl_ex_end(sp) || cl_vi_init(sp))
					goto err;
			} else
				if (cl_vi_end(sp) || cl_ex_init(sp))
					goto err;

			ev.e_event = E_START;
			if (v_event_handler(sp, &ev, &timeout))
				goto err;
		}

		/*
		 * Screen exit.  If exiting the last screen, shutdown
		 * the editor.
		 */
		if (F_ISSET(sp, S_EXIT | S_EXIT_FORCE)) {
			if ((next = sp->nextdisp) == NULL) {
				ev.e_event = E_STOP;
				if (v_event_handler(sp, &ev, &timeout))
					goto err;

				if (F_ISSET(sp, S_EX))
					(void)cl_ex_end(sp);
				else
					(void)cl_vi_end(sp);
			}
			if (screen_end(sp) || (sp = next) == NULL)
				break;
		}
	}

	rval = 0;
	if (0) {
err:		rval = 1;
		if (F_ISSET(sp, S_EX))
			(void)cl_ex_end(sp);
		else
			(void)cl_vi_end(sp);
	}

	/* Stop catching signals. */
	cl_sig_end(gp);

	/* Shutdown screens/global area. */
	v_end(gp);
	exit (rval);
}

/*
 * gs_init --
 *	Create and partially initialize the GS structure.
 */
static GS *
gs_init(name)
	char *name;
{
	CL_PRIVATE *clp;
	GS *gp;
	int fd;
	char *p;

	/* Figure out what our name is. */
	if ((p = strrchr(name, '/')) != NULL)
		name = p + 1;

	/* Allocate the global structure. */
	CALLOC_NOMSG(NULL, gp, GS *, 1, sizeof(GS));

	/* Allocate the CL private structure. */
	if (gp != NULL)
		CALLOC_NOMSG(NULL, clp, CL_PRIVATE *, 1, sizeof(CL_PRIVATE));
	if (gp == NULL || clp == NULL) {
		(void)fprintf(stderr, "%s: %s\n", name, strerror(errno));
		exit(1);
	}
	gp->cl_private = clp;

	/* Initialize the functions. */
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
	gp->scr_fmap = cl_fmap;
	gp->scr_getkey = cl_getkey;
	gp->scr_insertln = cl_insertln;
	gp->scr_interrupt = cl_interrupt;
	gp->scr_move = cl_move;
	gp->scr_msg = cl_msg;
	gp->scr_refresh = cl_refresh;
	gp->scr_resize = cl_resize;
	gp->scr_split = cl_split;
	gp->scr_suspend = cl_suspend;

	/*
	 * Set the G_STDIN_TTY flag.  It's purpose is to avoid setting and
	 * resetting the tty if the input isn't from there.
	 */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_STDIN_TTY);

	/*
	 * Set the G_TERMIOS_SET flag.  It's purpose is to avoid using the
	 * original_termios information (mostly special character values)
	 * if it's not valid.  We expect that if we've lost our controlling
	 * terminal that the open() (but not the tcgetattr()) will fail.
	 */
	if (F_ISSET(gp, G_STDIN_TTY)) {
		if (tcgetattr(STDIN_FILENO, &gp->original_termios) == -1)
			goto tcfail;
		F_SET(gp, G_TERMIOS_SET);
	} else if ((fd = open(_PATH_TTY, O_RDONLY, 0)) != -1) {
		if (tcgetattr(fd, &gp->original_termios) == -1) {
tcfail:			(void)fprintf(stderr,
			    "%s: tcgetattr: %s\n", name, strerror(errno));
			exit (1);
		}
		F_SET(gp, G_TERMIOS_SET);
		(void)close(fd);
	}

	gp->progname = name;
	return (gp);
}

#define	HANDLER(name, flag)						\
static void name(signo)	int signo; {					\
	F_SET(((CL_PRIVATE *)__global_list->cl_private), flag);		\
}
HANDLER(h_cont, CL_SIGCONT)
HANDLER(h_hup, CL_SIGHUP)
HANDLER(h_int, CL_SIGINT)
HANDLER(h_term, CL_SIGTERM)
HANDLER(h_winch, CL_SIGWINCH)

/*
 * cl_sig_init --
 *	Initialize signals.
 */
static int
cl_sig_init(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	GS *gp;
	struct sigaction act;

	gp = sp->gp;
	clp = CLP(sp);

	/* Initialize the signals. */
	(void)sigemptyset(&gp->blockset);

	/*
	 * Use sigaction(2), not signal(3), since we don't always want to
	 * restart system calls.  The example is when waiting for a command
	 * mode keystroke and SIGWINCH arrives.  Besides, you can't portably
	 * restart system calls.
	 */
#define	SETSIG(signal, handler, off) {					\
	if (sigaddset(&gp->blockset, signal))				\
		goto err;						\
	act.sa_handler = handler;					\
	sigemptyset(&act.sa_mask);					\
	act.sa_flags = 0;						\
	if (sigaction(signal, &act, &clp->oact[off]))			\
		goto err;						\
}
	SETSIG(SIGCONT, h_cont, INDX_CONT);
	SETSIG(SIGHUP, h_hup, INDX_HUP);
	SETSIG(SIGINT, h_int, INDX_INT);
	SETSIG(SIGTERM, h_term, INDX_TERM);
	SETSIG(SIGWINCH, h_winch, INDX_WINCH);
#undef SETSIG

	/* Notify generic code that signals need to be blocked. */
	F_SET(gp, G_SIGBLOCK);
	return (0);

err:	msgq(sp, M_SYSERR, "signal init");
	return (1);
}

/*
 * cl_sig_end --
 *	End signal setup.
 */
static void
cl_sig_end(gp)
	GS *gp;
{
	CL_PRIVATE *clp;

	/* If never initialized, don't reset. */
	if (!F_ISSET(gp, G_SIGBLOCK))
		return;

	clp = (CL_PRIVATE *)gp->cl_private;
	(void)sigaction(SIGCONT, NULL, &clp->oact[INDX_CONT]);
	(void)sigaction(SIGHUP, NULL, &clp->oact[INDX_HUP]);
	(void)sigaction(SIGINT, NULL, &clp->oact[INDX_INT]);
	(void)sigaction(SIGTERM, NULL, &clp->oact[INDX_TERM]);
	(void)sigaction(SIGWINCH, NULL, &clp->oact[INDX_WINCH]);
}

/*
 * killsig --
 *	Die with the proper exit status.
 */
static void
killsig(sp)
	SCR *sp;
{
	CL_PRIVATE *clp;
	int signo;

	clp = CLP(sp);
	if (F_ISSET(clp, CL_SIGHUP))
		signo = SIGHUP;
	else if (F_ISSET(clp, CL_SIGTERM))
		signo = SIGTERM;
	else
		abort();

	(void)signal(signo, SIG_DFL);
	(void)kill(getpid(), signo);
	/* NOTREACHED */

	exit (1);
}
