/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
char copyright[] =
"%Z% Copyright (c) 1992 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "$Id: main.c,v 5.62 1993/04/17 11:49:23 bostic Exp $ (Berkeley) $Date: 1993/04/17 11:49:23 $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "vi.h"
#include "excmd.h"

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
	EXCMDARG cmd;
	GS *gp;
	SCR *sp;
	int ch, eval;
	char *excmdarg, *errf, *myname, *p, *tag;

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
	HDR_INIT(gp->scrhdr, next, prev);
	HDR_INIT(gp->exfhdr, next, prev);
	if (tcgetattr(STDIN_FILENO, &gp->original_termios))
		err(1, "tcgetattr");
		
	/* Build and initialize the first/current screen. */
	if ((sp = malloc(sizeof(SCR))) == NULL)
		err(1, NULL);
	if (scr_init(NULL, sp))
		err(1, NULL);
	sp->gp = gp;		/* All screens point to the GS structure. */
	HDR_INIT(sp->seqhdr, next, prev);
	HDR_APPEND(sp, &gp->scrhdr, next, prev, SCR);

	set_window_size(sp, 0);	/* Set the window size. */

	if (opts_init(sp))	/* Options initialization. */
		goto err1;

	if (key_special(sp))	/* Keymaps. */
		goto err1;
#ifndef NO_DIGRAPH
	if (digraph_init(sp))	/* Digraph initialization. */
		goto err1;
#endif
	/* Set screen mode based on the program name. */
	if (!strcmp(myname, "ex") || !strcmp(myname, "nex"))
		F_SET(sp, S_MODE_EX);
	else if (!strcmp(myname, "view")) {
		O_SET(sp, O_READONLY);
		F_SET(sp, S_MODE_VI);
	} else
		F_SET(sp, S_MODE_VI);

	/* Convert old-style arguments into new-style ones. */
	obsolete(argv);

	/* Parse the arguments. */
	excmdarg = errf = tag = NULL;
	while ((ch = getopt(argc, argv, "c:emRrT:t:v")) != EOF)
		switch(ch) {
		case 'c':		/* Run the command. */
			excmdarg = optarg;
			break;
		case 'e':		/* Ex mode. */
			F_SET(sp, S_MODE_EX);
			break;
#ifndef NO_ERRLIST
		case 'm':		/* Error list. */
			errf = optarg;
			break;
#endif
		case 'R':		/* Readonly. */
			O_SET(sp,O_READONLY);
			break;
		case 'r':		/* Recover. */
			errx(1, "recover option not yet implemented");
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((gp->tracefp = fopen(optarg, "w")) == NULL)
				err(1, "%s", optarg);
			(void)fprintf(gp->tracefp,
			    "\n===\ntrace: open %s\n", optarg);
			break;
#endif
		case 't':		/* Tag. */
			tag = optarg;
			break;
		case 'v':		/* Vi mode. */
			F_SET(sp, S_MODE_VI);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * Source the system, ~user and local .exrc files.
	 *
	 * XXX
	 * Check the correct order for these.
	 */
	(void)ex_cfile(sp, NULL, _PATH_SYSEXRC, 0);
	if ((p = getenv("HOME")) != NULL && *p) {
		char path[MAXPATHLEN];

		(void)snprintf(path, sizeof(path), "%s/.exrc", p);
		(void)ex_cfile(sp, NULL, path, 0);
	}
	if (O_ISSET(sp, O_EXRC))
		(void)ex_cfile(sp, NULL, _PATH_EXRC, 0);

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL)
			msgq(sp, M_ERR, "Error: %s", strerror(errno));
		else {
			(void)ex_cstring(sp, NULL, p, strlen(p));
			free(p);
		}

	/* Any remaining arguments are file names. */
	if (argc)
		file_set(sp, argc, argv);

	/* Use an error list file if specified. */
#ifndef NO_ERRLIST
	if (errf != NULL) {
		SETCMDARG(cmd, C_ERRLIST, 0, OOBLNO, 0, 0, errf);
		if (ex_errlist(sp, NULL, &cmd))
			goto err1;
	} else
#endif
	/* Use a tag file if specified. */
	if (tag != NULL) {
		SETCMDARG(cmd, C_TAG, 0, OOBLNO, 0, 0, tag);
		if (ex_tagpush(sp, NULL, &cmd))
			goto err1;
	} else
		sp->enext = file_first(sp, 1);

	/* Get a real EXF structure. */
	if ((sp->ep = file_start(sp, sp->enext)) == NULL)
		goto err1;

	/* Do commands from the command line. */
	if (excmdarg != NULL)
		(void)ex_cstring(sp, sp->ep, excmdarg, strlen(excmdarg));

	/* Call a screen. */
	switch(F_ISSET(sp, S_MODE_EX | S_MODE_VI)) {
	case S_MODE_EX:
		if (sex(sp, sp->ep))
			goto err1;
		break;
	case S_MODE_VI:
		if (svi(sp, sp->ep))
			goto err1;
		break;
	default:
		abort();
	}

	/*
	 * NOTE: sp may be GONE by now.
	 * Only gp can be trusted.
	 */

	/* Yeah, I don't like it either. */
	if (0)
err1:		eval = 1;

	/* Reset anything that needs resetting. */
	reset(gp);

	/* Flush any left-over error messages. */
	msgflush(gp);

	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &gp->original_termios))
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
	 *	Change "+" into "-c$".
	 *	Change "+[0-9]*" into "-c:[0-9]".
	 */
	for (myname = argv[0]; *++argv;)
		if (argv[0][0] == '+')
			if (argv[0][1] == '\0') {
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
				if ((argv[0] = malloc(len + 3)) == NULL)
					err(1, NULL);
				argv[0][0] = '-';
				argv[0][1] = 'c';
				argv[0][2] = ':';
				(void)strcpy(argv[0] + 3, p + 1);
			}
			
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: vi [-eRrv] [-c command] [-m file] [-t tag]\n");
	exit(1);
}
