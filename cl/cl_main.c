/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: cl_main.c,v 10.16 1995/10/31 10:52:15 bostic Exp $ (Berkeley) $Date: 1995/10/31 10:52:15 $";
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

#include "../common/common.h"
#include "../common/pathnames.h"
#include "cl.h"

GS *__global_list;				/* GLOBAL: List of screens. */
sigset_t __sigblockset;				/* GLOBAL: Blocked signals. */

static GS	*gs_init __P((char *));
static int	 sig_init __P((GS *));
static void	 sig_end __P((GS *));
static void	 term_init __P((char *));

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
	CL_PRIVATE *clp;
	GS *gp;
	recno_t rows;
	size_t cols;
	int rval;
	char **p_av, **t_av, *ttype;

	/* If loaded at 0 and jumping through a NULL pointer, stop. */
	if (reenter++)
		abort();

	/* Create and initialize the global structure. */
	__global_list = gp = gs_init(argv[0]);
	clp = GCLP(gp);

#ifdef NOT_NEEDED_FOR_CURSES
	/*
	 * Strip out any arguments that vi isn't going to understand.  There's
	 * no way to portably call getopt twice, so arguments parsed here must
	 * be removed from the argument list.
	 */
	for (p_av = t_av = argv;;) {
		if (*t_av == NULL) {
			*p_av = NULL;
			break;
		}
		*p_av++ = *t_av++;
	}
#endif

	/*
	 * Initialize the terminal information.
	 *
	 * We have to know what terminal it is from the start, since we may
	 * have to use termcap/terminfo to find out how big the screen is.
	 */
	if ((ttype = getenv("TERM")) == NULL)
		ttype = "unknown";
	term_init(ttype);

	/* Figure out how big the screen is. */
	if (cl_ssize(NULL, 0, &rows, &cols, NULL))
		exit (1);

	/* Ex wants stdout to be buffered. */
	(void)setvbuf(stdout, NULL, _IOFBF, 0);

	/* Start catching signals. */
	if (sig_init(gp))
		exit (1);

	/* Run ex/vi. */
	rval = editor(gp, argc, argv, rows, cols);

	/* Clean up signals. */
	sig_end(gp);

	/* Clean up the terminal. */
	(void)cl_quit(gp);

	/* If a killer signal arrived, pretend we just got it. */
	if (clp->killersig) {
		(void)signal(clp->killersig, SIG_DFL);
		(void)kill(getpid(), clp->killersig);
		/* NOTREACHED */
	}

	/* Free the global and CL private areas. */
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(clp);
	free(gp);
#endif

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

	/* Initialize the list of curses functions. */
	gp->scr_addstr = cl_addstr;
	gp->scr_attr = cl_attr;
	gp->scr_baud = cl_baud;
	gp->scr_bell = cl_bell;
	gp->scr_busy = NULL;
	gp->scr_clrtoeol = cl_clrtoeol;
	gp->scr_cursor = cl_cursor;
	gp->scr_deleteln = cl_deleteln;
	gp->scr_event = cl_event;
	gp->scr_ex_adjust = cl_ex_adjust;
	gp->scr_fmap = cl_fmap;
	gp->scr_insertln = cl_insertln;
	gp->scr_keyval = cl_keyval;
	gp->scr_move = cl_move;
	gp->scr_msg = NULL;
	gp->scr_optchange = cl_optchange;
	gp->scr_refresh = cl_refresh;
	gp->scr_rename = cl_rename;
	gp->scr_screen = cl_screen;
	gp->scr_suspend = cl_suspend;
	gp->scr_usage = cl_usage;

	/*
	 * Set the G_STDIN_TTY flag.  It's purpose is to avoid setting and
	 * resetting the tty if the input isn't from there.
	 */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_STDIN_TTY);

	/*
	 * We expect that if we've lost our controlling terminal that the
	 * open() (but not the tcgetattr()) will fail.
	 */
	if (F_ISSET(gp, G_STDIN_TTY)) {
		if (tcgetattr(STDIN_FILENO, &clp->orig) == -1)
			goto tcfail;
	} else if ((fd = open(_PATH_TTY, O_RDONLY, 0)) != -1) {
		if (tcgetattr(fd, &clp->orig) == -1) {
tcfail:			(void)fprintf(stderr,
			    "%s: tcgetattr: %s\n", name, strerror(errno));
			exit (1);
		}
		(void)close(fd);
	}

	gp->progname = name;
	return (gp);
}

/*
 * term_init --
 *	Initialize terminal information.
 */
static void
term_init(ttype)
	char *ttype;
{
	int err;

	/* Set up the terminal database information. */
	setupterm(ttype, STDOUT_FILENO, &err);
	switch (err) {
	case -1:
		(void)fprintf(stderr, "No terminal database found");
		exit (1);
	case 0:
		(void)fprintf(stderr, "%s: unknown terminal type", ttype);
		exit (1);
	}
}

#define	GLOBAL_CLP \
	CL_PRIVATE *clp = GCLP(__global_list);
static void
h_hup(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGHUP);
	clp->killersig = SIGHUP;
}

static void
h_int(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGINT);
}

static void
h_term(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGTERM);
	clp->killersig = SIGTERM;
}

static void
h_winch(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGWINCH);
}
#undef	GLOBAL_CLP

/*
 * sig_init --
 *	Initialize signals.
 */
static int
sig_init(gp)
	GS *gp;
{
	CL_PRIVATE *clp;
	struct sigaction act;

	clp = GCLP(gp);

	(void)sigemptyset(&__sigblockset);

	/*
	 * Use sigaction(2), not signal(3), since we don't always want to
	 * restart system calls.  The example is when waiting for a command
	 * mode keystroke and SIGWINCH arrives.  Besides, you can't portably
	 * restart system calls (thanks, POSIX!).
	 */
#define	SETSIG(signal, handler, off) {					\
	if (sigaddset(&__sigblockset, signal))				\
		goto err;						\
	act.sa_handler = handler;					\
	sigemptyset(&act.sa_mask);					\
	act.sa_flags = 0;						\
	if (sigaction(signal, &act, &clp->oact[off]))			\
		goto err;						\
}
	SETSIG(SIGHUP, h_hup, INDX_HUP);
	SETSIG(SIGINT, h_int, INDX_INT);
	SETSIG(SIGTERM, h_term, INDX_TERM);
	SETSIG(SIGWINCH, h_winch, INDX_WINCH);
#undef SETSIG
	return (0);

err:	(void)fprintf(stderr, "%s: %s\n", gp->progname, strerror(errno));
	return (1);
}

/*
 * sig_end --
 *	End signal setup.
 */
static void
sig_end(gp)
	GS *gp;
{
	CL_PRIVATE *clp;

	clp = GCLP(gp);
	(void)sigaction(SIGHUP, NULL, &clp->oact[INDX_HUP]);
	(void)sigaction(SIGINT, NULL, &clp->oact[INDX_INT]);
	(void)sigaction(SIGTERM, NULL, &clp->oact[INDX_TERM]);
	(void)sigaction(SIGWINCH, NULL, &clp->oact[INDX_WINCH]);
}
