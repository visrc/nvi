/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char copyright[] =
"%Z% Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "$Id: main.c,v 8.42 1993/11/18 08:17:05 bostic Exp $ (Berkeley) $Date: 1993/11/18 08:17:05 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"
#include "tag.h"

static GS	*gs_init __P((void));
static void	 h_hup __P((int));
static void	 h_term __P((int));
static void	 h_winch __P((int));
static void	 msgflush __P((GS *));
static void	 obsolete __P((char *[]));
static void	 reset __P((GS *));
static void	 usage __P((void));

GS *__global_list;			/* GLOBAL: List of screens. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	static int reenter;		/* STATIC: Re-entrancy check. */
	struct sigaction act;
	struct stat sb;
	GS *gp;
	FREF *frp;
	SCR *nsp, *sp;
	u_int flags;
	int ch, eval, flagchk, readonly, snapshot;
	char *excmdarg, *myname, *p, *rec_f, *tag_f, *trace_f, *wsizearg;
	char *av[2], path[MAXPATHLEN];

	/* Stop if indirecting through a NULL pointer. */
	if (reenter++)
		abort();

	/* Set screen type and mode based on the program name. */
	readonly = 0;
	if ((myname = strrchr(*argv, '/')) == NULL)
		myname = *argv;
	else
		++myname;
	if (!strcmp(myname, "ex") || !strcmp(myname, "nex"))
		LF_INIT(S_EX);
	else {
		/* View is readonly. */
		if (!strcmp(myname, "view"))
			readonly = 1;
		LF_INIT(S_VI_CURSES);
	}

	/* Convert old-style arguments into new-style ones. */
	obsolete(argv);

	/* Parse the arguments. */
	flagchk = '\0';
	excmdarg = rec_f = tag_f = trace_f = wsizearg = NULL;
	snapshot = 1;
	while ((ch = getopt(argc, argv, "c:elmRr:sT:t:vw:x:")) != EOF)
		switch (ch) {
		case 'c':		/* Run the command. */
			excmdarg = optarg;
			break;
		case 'e':		/* Ex mode. */
			LF_CLR(S_SCREENS);
			LF_SET(S_EX);
			break;
		case 'l':
			if (flagchk != '\0' && flagchk != 'l')
				errx(1,
				    "only one of -%c and -l may be specified.",
				    flagchk);
			flagchk = 'l';
			break;
		case 'R':		/* Readonly. */
			readonly = 1;
			break;
		case 'r':		/* Recover. */
			if (flagchk == 'r')
				errx(1,
				    "only one recovery file may be specified.");
			if (flagchk != '\0')
				errx(1,
				    "only one of -%c and -r may be specified.",
				    flagchk);
			flagchk = 'r';
			rec_f = optarg;
			break;
		case 's':		/* No snapshot. */
			snapshot = 0;
			break;
		case 'T':		/* Trace. */
			trace_f = optarg;
			break;
		case 't':		/* Tag. */
			if (flagchk == 't')
				errx(1,
				    "only one tag file may be specified.");
			if (flagchk != '\0')
				errx(1,
				    "only one of -%c and -t may be specified.",
				    flagchk);
			flagchk = 't';
			tag_f = optarg;
			break;
		case 'v':		/* Vi mode. */
			LF_CLR(S_SCREENS);
			LF_SET(S_VI_CURSES);
			break;
		case 'w':
			wsizearg = optarg;
			break;
		case 'x':
			if (!strcmp(optarg, "aw")) {
				F_CLR(sp, S_SCREENS);
				F_SET(sp, S_VI_XAW);
				break;
			}
			/* FALLTHROUGH */
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Build and initialize the GS structure. */
	__global_list = gp = gs_init();
		
	if (snapshot)
		F_SET(gp, G_SNAPSHOT);

	/* Build and initialize the first/current screen. */
	if (screen_init(NULL, &sp, flags))
		goto err1;

	if (trace_f != NULL) {
#ifdef DEBUG
		if ((gp->tracefp = fopen(optarg, "w")) == NULL)
			err(1, "%s", optarg);
		(void)fprintf(gp->tracefp, "\n===\ntrace: open %s\n", optarg);
#else
		msgq(sp, M_ERR, "-T support not compiled into this version.");
#endif
	}

	if (set_window_size(sp, 0, 0))	/* Set the window size. */
		goto err1;

	if (opts_init(sp))		/* Options initialization. */
		goto err1;
	if (readonly)
		O_SET(sp, O_READONLY);
	if (wsizearg != NULL) {
		av[0] = path;
		av[1] = NULL;
		if (strtol(optarg, &p, 10) < 0 || *p)
			errx(1, "illegal window size -- %s", optarg);
		(void)snprintf(path, sizeof(path), "window=%s", optarg);
		if (opts_set(sp, av))
			 msgq(sp, M_ERR,
			     "Unable to set command line window option");
	}

	/* Keymaps, special keys, must follow option initializations. */
	if (term_init(sp))
		goto err1;

#ifdef	DIGRAPHS
	if (digraph_init(sp))		/* Digraph initialization. */
		goto err1;
#endif

	/*
	 * Source the system, environment, ~user and local .exrc values.
	 * If the environment exists, vi historically doesn't check ~user.
	 * This is done before the file is read in because things in the
	 * .exrc information can set, for example, the recovery directory.
	 */
	if (!stat(_PATH_SYSEXRC, &sb))
		(void)ex_cfile(sp, NULL, _PATH_SYSEXRC);

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			goto err1;
		} else {
			(void)ex_cstring(sp, NULL, p, strlen(p));
			free(p);
		}
	else if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/%s", p, _PATH_NEXRC);
		if (!stat(path, &sb))
			(void)ex_cfile(sp, NULL, path);
		else {
			(void)snprintf(path,
			    sizeof(path), "%s/%s", p, _PATH_EXRC);
			if (!stat(path, &sb))
				(void)ex_cfile(sp, NULL, path);
		}
	}
	if (O_ISSET(sp, O_EXRC) && !stat(_PATH_EXRC, &sb))
		(void)ex_cfile(sp, NULL, _PATH_EXRC);

	/* List recovery files if -l specified. */
	if (flagchk == 'l')
		exit(rcv_list(sp));

	/* Use a tag file or recovery file if specified. */
	if (tag_f != NULL && ex_tagfirst(sp, tag_f))
		goto err1;
	else if (rec_f != NULL && rcv_read(sp, rec_f))
		goto err1;

	/* Append any remaining arguments as file names. */
	if (*argv != NULL)
		for (; *argv != NULL; ++argv)
			if (file_add(sp, NULL, *argv, 0) == NULL)
				goto err1;

	/*
	 * If no recovery or tag file, get an EXF structure.
	 * If no argv file, use a temporary file.
	 */
	if (tag_f == NULL && rec_f == NULL) {
		if ((frp = file_first(sp, 1)) == NULL &&
		    (frp = file_add(sp, NULL, NULL, 0)) == NULL)
			goto err1;
		if (file_init(sp, frp, NULL, 0))
			goto err1;
	}

	/*
	 * Initialize the signals.  Use sigaction(2), not signal(3), because
	 * we don't want to always restart system calls on 4BSD systems.  It
	 * would be nice in some cases to restart system calls, but SA_RESTART
	 * is a 4BSD extension so we can't use it.
	 *
	 * SIGWINCH, SIGHUP, SIGTERM:
	 *	Catch and set a global bit.
	 */
	act.sa_handler = h_hup;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGHUP, &act, NULL);
	act.sa_handler = h_term;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGTERM, &act, NULL);
	act.sa_handler = h_winch;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGWINCH, &act, NULL);

	/*
	 * SIGQUIT:
	 *	Always ignore.
	 */
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	(void)sigaction(SIGQUIT, &act, NULL);

	/*
	 * If there's an initial command, push it on the command stack.
	 * Historically, it was always an ex command, not vi in vi mode
	 * or ex in ex mode.  So, make it look like an ex command to vi.
	 */
	if (excmdarg != NULL)
		if (IN_EX_MODE(sp)) {
			if (term_push(sp, sp->gp->tty,
			    excmdarg, strlen(excmdarg)))
				goto err1;
		} else if (IN_VI_MODE(sp)) {
			if (term_push(sp, sp->gp->tty, "\n", 1))
				goto err1;
			if (term_push(sp, sp->gp->tty,
			    excmdarg, strlen(excmdarg)))
				goto err1;
			if (term_push(sp, sp->gp->tty, ":", 1))
				goto err1;
		}
		
	/* Vi reads from the terminal. */
	if (!F_ISSET(gp, G_ISFROMTTY) && !F_ISSET(sp, S_EX)) {
		msgq(sp, M_ERR, "Vi's standard input must be a terminal.");
		goto err1;
	}

	for (;;) {
		if (sp->s_edit(sp, sp->ep, &nsp))
			goto err2;

		/* If we're done, nsp will be NULL. */
		if ((sp = nsp) == NULL)
			break;

		/*
		 * The screen type may have changed -- reinitialize the
		 * functions in case it has.
		 */
		switch (F_ISSET(sp, S_SCREENS)) {
		case S_EX:
			if (sex_screen_init(sp))
				return (1);
			break;
		case S_VI_CURSES:
			if (svi_screen_init(sp))
				return (1);
			break;
		case S_VI_XAW:
			if (xaw_screen_init(sp))
				return (1);
			break;
		default:
			abort();
		}
	}

	/*
	 * Two error paths.  The first means that something failed before
	 * we called a screen routine.  Swap the message pointers between
	 * the SCR and the GS, so messages get displayed.  The second is
	 * something failed in a screen.  NOTE: sp may be GONE when the
	 * screen returns, so only the gp can be trusted.
	 */
	eval = 0;
	if (0) {
err1:		gp->msgp = sp->msgp;
err2:		eval = 1;
	}

	/* Reset anything that needs resetting. */
	reset(gp);

	/* Flush any left-over error messages. */
	msgflush(gp);

	/*
	 * DON'T FREE THE GLOBAL STRUCTURE -- WE DIDN'T TURN
	 * OFF SIGNALS/TIMERS, SO IT MAY STILL BE REFERENCED.
	 */

	/* Make absolutely sure that the modes are restored correctly. */
	if (F_ISSET(gp, G_ISFROMTTY) &&
	    tcsetattr(STDIN_FILENO, TCSADRAIN, &gp->original_termios))
		err(1, "tcsetattr");
	exit(eval);
}

/*
 * gs_init --
 *	Build and initialize the GS structure.
 */
static GS *
gs_init()
{
	GS *gp;
	int fd;

	if ((gp = malloc(sizeof(GS))) == NULL)
		err(1, NULL);
	memset(gp, 0, sizeof(GS));

	/* Structures shared by screens so stored in the GS structure. */
	if ((gp->key = malloc(sizeof(IBUF))) == NULL)
		err(1, NULL);
	memset(gp->key, 0, sizeof(IBUF));
	if ((gp->tty = malloc(sizeof(IBUF))) == NULL)
		err(1, NULL);
	memset(gp->tty, 0, sizeof(IBUF));

	/* Set a flag if we're reading from the tty. */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_ISFROMTTY);

	/*
	 * XXX
	 * Set a flag and don't do terminal sets/resets if the input isn't
	 * from a tty.  Under all circumstances put reasonable things into
	 * the original_termios field, as some routines (seq.c:seq_save()
	 * and term.c:term_init()) want values for special characters.
	 */
	if (F_ISSET(gp, G_ISFROMTTY)) {
		if (tcgetattr(STDIN_FILENO, &gp->original_termios))
			err(1, "tcgetattr");
	} else {
		if ((fd = open(_PATH_TTY, O_RDONLY, 0)) == -1)
			err(1, "%s", _PATH_TTY);
		if (tcgetattr(fd, &gp->original_termios))
			err(1, "tcgetattr");
		(void)close(fd);
	}

	return (gp);
}

/*
 * reset --
 *	Reset any changed global state.
 */
static void
reset(gp)
	GS *gp;
{
	char *tty;

	if (gp->flags & G_SETMODE)			/* O_MESG */
		if ((tty = ttyname(STDERR_FILENO)) == NULL)
			warn("ttyname");
		else if (chmod(tty, gp->origmode) < 0)
			warn("%s", tty);
}

/*
 * msgflush --
 *	Flush any remaining messages.
 */
static void
msgflush(gp)
	GS *gp;
{
	MSG *mp;

	/* Ring the bell. */
	if (F_ISSET(gp, G_BELLSCHED))
		(void)fprintf(stderr, "\07");		/* \a */

	/* Display the messages. */
	for (mp = gp->msgp;
	    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->next) 
		(void)fprintf(stderr, "%.*s\n", (int)mp->len, mp->mbuf);
}

/*
 * h_hup --
 *	Handle SIGHUP.
 */
static void
h_hup(signo)
	int signo;
{
	F_SET(__global_list, G_SIGHUP);

	/*
	 * If we're asleep, just die.
	 *
	 * XXX
	 * This isn't right if the windows are independent.
	 */
	if (F_ISSET(__global_list, G_SLEEPING))
		rcv_hup();
}

/*
 * h_term --
 *	Handle SIGTERM.
 */
static void
h_term(signo)
	int signo;
{
	F_SET(__global_list, G_SIGTERM);

	/*
	 * If we're asleep, just die.
	 *
	 * XXX
	 * This isn't right if the windows are independent.
	 */
	if (F_ISSET(__global_list, G_SLEEPING))
		rcv_term();
}

/*
 * h_winch --
 *	Handle SIGWINCH.
 */
static void
h_winch(signo)
	int signo;
{
	F_SET(__global_list, G_SIGWINCH);
}

static void
obsolete(argv)
	char *argv[];
{
	size_t len;
	char *p, *myname;

	/*
	 * Translate old style arguments into something getopt will like.
	 * Make sure it's not text space memory, because ex changes the
	 * strings.
	 *	Change "+" into "-c$".
	 *	Change "+<anything else>" into "-c<anything else>".
	 *	Change "-r" into "-l"
	 */
	for (myname = argv[0]; *++argv;)
		if (argv[0][0] == '+') {
			if (argv[0][1] == '\0') {
				if ((argv[0] = malloc(4)) == NULL)
					err(1, NULL);
				(void)strcpy(argv[0], "-c$");
			} else  {
				p = argv[0];
				len = strlen(argv[0]);
				if ((argv[0] = malloc(len + 2)) == NULL)
					err(1, NULL);
				argv[0][0] = '-';
				argv[0][1] = 'c';
				(void)strcpy(argv[0] + 2, p + 1);
			}
		}
		else if (argv[0][0] == '-' &&
		    argv[0][1] == 'r' && argv[0][2] == '\0' && argv[1] == NULL)
			argv[0][1] = 'l';
}

static void
usage()
{
	(void)fprintf(stderr, "%s%s\n", 
	    "usage: vi [-eRsv] [-c command] [-m file] [-r file] ",
	    "[-t tag] [-w size] [-x aw]");
	exit(1);
}
