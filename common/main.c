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
static char sccsid[] = "$Id: main.c,v 8.29 1993/11/01 11:58:58 bostic Exp $ (Berkeley) $Date: 1993/11/01 11:58:58 $";
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
#include "recover.h"
#include "pathnames.h"
#include "tag.h"

static void h_hup __P((int));
static void h_term __P((int));
static void h_winch __P((int));
static void msgflush __P((GS *));
static void obsolete __P((char *[]));
static void reset __P((GS *));
static void usage __P((void));

GS *__global_list;			/* GLOBAL: List of screens. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	static int reenter;		/* STATIC: Re-entrancy check. */
	enum { EX_SCR, VI_CURSES_SCR, VI_XAW_SCR } scr_type;
	struct sigaction act;
	struct stat sb;
	GS *gp;
	FREF *frp;
	SCR *nsp, *sp;
	int ch, fd, flagchk, eval;
	char *excmdarg, *errf, *myname, *p, *rfname, *tfname;
	char *av[2], path[MAXPATHLEN];

	/* Stop if indirecting through a NULL pointer. */
	if (reenter++)
		abort();

	eval = 0;

	/* Figure out the program name. */
	if ((myname = strrchr(*argv, '/')) == NULL)
		myname = *argv;
	else
		++myname;

	/* Build and initialize the GS structure. */
	if ((gp = malloc(sizeof(GS))) == NULL)
		err(1, NULL);
	__global_list = gp;
	memset(gp, 0, sizeof(GS));

	/* Structures shared by screens so stored in the GS structure. */
	if ((gp->key = malloc(sizeof(IBUF))) == NULL)
		err(1, NULL);
	memset(gp->key, 0, sizeof(IBUF));
	if ((gp->tty = malloc(sizeof(IBUF))) == NULL)
		err(1, NULL);
	memset(gp->tty, 0, sizeof(IBUF));

	if ((gp->cuts = malloc((UCHAR_MAX + 2) * sizeof(CB))) == NULL)
		err(1, NULL);
	memset(gp->cuts, 0, (UCHAR_MAX + 2) * sizeof(CB));

	/* Set a flag if we're reading from the tty. */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_ISFROMTTY);

	/*
	 * XXX
	 * Set a flag and don't do terminal sets/resets if the input isn't
	 * from a tty.  This should eventually be fixed so that users can
	 * pipe input to nvi.  Note, under all circumstances put reasonable
	 * things into the original_termios field, as seq.c:seq_save() and
	 * term.c:term_init() want values for special characters.
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
		
	/* Build and initialize the first/current screen. */
	if ((sp = malloc(sizeof(SCR))) == NULL)
		err(1, NULL);
	if (screen_init(NULL, sp))
		err(1, NULL);
	HDR_INIT(sp->seqhdr, next, prev);

	sp->gp = gp;		/* All screens point to the GS structure. */

	if (set_window_size(sp, 0, 0))	/* Set the window size. */
		goto err1;

	if (opts_init(sp))		/* Options initialization. */
		goto err1;

	/*
	 * Keymaps, special keys.
	 * Must follow options and sequence map initializations.
	 */
	if (term_init(sp))
		goto err1;

#ifndef NO_DIGRAPH
	if (digraph_init(sp))		/* Digraph initialization. */
		goto err1;
#endif
	/* Set screen type and mode based on the program name. */
	if (!strcmp(myname, "ex") || !strcmp(myname, "nex")) {
		F_SET(sp, S_MODE_EX);
		scr_type = EX_SCR;
	} else {
		if (!strcmp(myname, "view"))
			O_SET(sp, O_READONLY);
		F_SET(sp, S_MODE_VI);
		scr_type = VI_CURSES_SCR;
	}

	/* Convert old-style arguments into new-style ones. */
	obsolete(argv);

	F_SET(gp, G_SNAPSHOT);		/* Default to a snapshot. */

	/* Parse the arguments. */
	flagchk = '\0';
	excmdarg = errf = rfname = tfname = NULL;
	while ((ch = getopt(argc, argv, "c:elmRr:sT:t:vw:x:")) != EOF)
		switch (ch) {
		case 'c':		/* Run the command. */
			excmdarg = optarg;
			break;
		case 'e':		/* Ex mode. */
			F_SET(sp, S_MODE_EX);
			break;
		case 'l':
			if (flagchk != '\0' && flagchk != 'l')
				errx(1,
				    "only one of -%c and -l may be specified.",
				    flagchk);
			flagchk = 'l';
			break;
		case 'R':		/* Readonly. */
			O_SET(sp,O_READONLY);
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
			rfname = optarg;
			break;
		case 's':		/* No snapshot. */
			F_CLR(gp, G_SNAPSHOT);
			break;
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((gp->tracefp = fopen(optarg, "w")) == NULL)
				err(1, "%s", optarg);
			(void)fprintf(gp->tracefp,
			    "\n===\ntrace: open %s\n", optarg);
			break;
#endif
		case 't':		/* Tag. */
			if (flagchk == 't')
				errx(1,
				    "only one tag file may be specified.");
			if (flagchk != '\0')
				errx(1,
				    "only one of -%c and -t may be specified.",
				    flagchk);
			flagchk = 't';
			tfname = optarg;
			break;
		case 'v':		/* Vi mode. */
			F_SET(sp, S_MODE_VI);
			if (scr_type == EX_SCR)
				scr_type = VI_CURSES_SCR;
			break;
		case 'w':
			av[0] = path;
			av[1] = NULL;
			if (strtol(optarg, &p, 10) < 0 || *p)
				errx(1, "illegal window size -- %s", optarg);
			(void)snprintf(path,
			    sizeof(path), "window=%s", optarg);
			if (opts_set(sp, av))
				 msgq(sp, M_ERR,
			     "Unable to set command line window option");
			break;
		case 'x':
			if (!strcmp(optarg, "aw")) {
				scr_type = VI_XAW_SCR;
				break;
			}
			/* FALLTHROUGH */
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

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
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
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
	if (tfname != NULL && ex_tagfirst(sp, tfname))
		goto err1;
	else if (rfname != NULL && rcv_read(sp, rfname))
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
	if (tfname == NULL && rfname == NULL) {
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
		switch (F_ISSET(sp, S_MODE_EX | S_MODE_VI)) {
		case S_MODE_EX:
			if (term_push(sp, sp->gp->tty,
			    excmdarg, strlen(excmdarg)))
				goto err1;
			break;
		case S_MODE_VI:
			if (term_push(sp, sp->gp->tty, "\n", 1))
				goto err1;
			if (term_push(sp, sp->gp->tty,
			    excmdarg, strlen(excmdarg)))
				goto err1;
			if (term_push(sp, sp->gp->tty, ":", 1))
				goto err1;
			break;
		default:
			abort();
			/* NOTREACHED */
		}
		
	/* Vi reads from the terminal. */
	if (!F_ISSET(gp, G_ISFROMTTY) && !F_ISSET(sp, S_MODE_EX)) {
		msgq(sp, M_ERR, "Vi's standard input must be a terminal.");
		goto err1;
	}

	/*
	 * Call a screen.  There's two things that we look at.  In the
	 * screen flags word there's a bit if it's a vi or ex screen.
	 * Since there are multiple vi screens, we use scr_type to decide.
	 */
	for (; sp != NULL; sp = nsp)
		if (F_ISSET(sp, S_MODE_EX)) {
			if (sex(sp, sp->ep, &nsp))
				goto err2;
		} else
			switch (scr_type) {
			case EX_SCR:
				abort();
				/* NOTREACHED */
			case VI_CURSES_SCR:
				if (svi(sp, sp->ep, &nsp))
					goto err2;
				break;
			case VI_XAW_SCR:
				if (xaw(sp, sp->ep, &nsp))
					goto err2;
				break;
			default:
				abort();
				/* NOTREACHED */
			}

	/*
	 * Two error paths.  The first means that something failed before
	 * we called a screen routine.  Swap the message pointers between
	 * the SCR and the GS, so messages get displayed.  The second is
	 * something failed in a screen.  NOTE: sp may be GONE when the
	 * screen returns, so only the gp can be trusted.
	 */
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
	if (F_ISSET(gp, S_BELLSCHED))
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
	 *	Change "+/command" into "-ccommand".
	 *	Change "+" and "+$" into "-c$".
	 *	Change "+[0-9]*" into "-c[0-9]".
	 *	Change "-r" into "-l"
	 */
	for (myname = argv[0]; *++argv;)
		if (argv[0][0] == '+') {
			if (argv[0][1] == '\0' || argv[0][1] == '$') {
				if ((argv[0] = malloc(4)) == NULL)
					err(1, NULL);
				memmove(argv[0], "-c$", 4);
			} else if (argv[0][1] == '/') {
				p = argv[0];
				len = strlen(argv[0]);
				if ((argv[0] = malloc(len + 3)) == NULL)
					err(1, NULL);
				argv[0][0] = '-';
				argv[0][1] = 'c';
				(void)strcpy(argv[0] + 2, p + 1);
			} else if (isdigit(argv[0][1])) {
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
