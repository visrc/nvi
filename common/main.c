/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
char copyright[] =
"%Z% Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "$Id: main.c,v 5.55 1993/03/25 14:59:11 bostic Exp $ (Berkeley) $Date: 1993/03/25 14:59:11 $";
#endif /* not lint */

#include <sys/param.h>

#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
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
#include "options.h"
#include "pathnames.h"
#include "seq.h"
#include "term.h"

static void err __P((const char *, ...));
static void obsolete __P((char *[]));
static void reset __P((SCR *));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	static int reenter;
	EXCMDARG cmd;
	EXF *ep;
	GS *gp;
	SCR *sp;
	int ch, eval, mode;
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
		err("%s", strerror(errno));
	memset(gp, 0, sizeof(GS));
	HEADER_INIT(gp->scrhdr, next, prev, SCR);
	HEADER_INIT(gp->exfhdr, next, prev, EXF);
	if (tcgetattr(STDIN_FILENO, &gp->original_termios))
		err("%s: tcgetttr: %s", myname, strerror(errno));
		
	/* Build and initialize the current screen. */
	if ((sp = malloc(sizeof(SCR))) == NULL)
		err("%s", strerror(errno));
	if (scr_def(NULL, sp))
		goto err1;
	HEADER_INIT(sp->seqhdr, next, prev, SEQ);
	INSERT_TAIL(sp, (SCR *)&gp->scrhdr, next, prev, SCR);

	sp->gp = gp;		/* All screens point to the GS structure. */

	if (gb_init(sp))	/* Key input initialization. */
		goto err1;
#ifndef NO_DIGRAPH
	if (digraph_init(sp))	/* Digraph initialization. */
		goto err1;
#endif
	if (opts_init(sp))	/* Options initialization. */
		goto err1;

	set_window_size(sp, 0);	/* Set the window size. */

	/* Set screen mode based on the program name. */
	if (!strcmp(myname, "ex") || !strcmp(myname, "nex"))
		F_SET(sp, S_MODE_EX);
	else if (!strcmp(myname, "view")) {
		SET(O_READONLY)
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
			SET(O_READONLY);
			break;
		case 'r':		/* Recover. */
			err("%s: recover option not yet implemented", myname);
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((gp->tracefp = fopen(optarg, "w")) == NULL)
				err("%s: %s: %s",
				    myname, optarg, strerror(errno));
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
	if (ISSET(O_EXRC))
		(void)ex_cfile(sp, NULL, _PATH_EXRC, 0);

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL)
			msgq(sp, M_ERROR, "Error: %s", strerror(errno));
		else {
			(void)ex_cstring(sp, NULL, (u_char *)p, strlen(p), 1);
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
	if ((ep = file_start(sp, sp->enext)) == NULL)
		goto err1;

	/* Do commands from the command line. */
	if (excmdarg != NULL)
		(void)ex_cstring(sp, ep,
		    (u_char *)excmdarg, strlen(excmdarg), 0);

	/*
	 * XXX
	 * Turn on signal handling.
	 */
	(void)signal(SIGHUP, onhup);
	(void)signal(SIGINT, SIG_IGN);

	do {
		/* Initialize the screen. */
		switch(sp->flags & (S_MODE_EX | S_MODE_VI)) {
		case S_MODE_EX:
			if (ex_s_init(sp, ep))
				goto err1;
			break;
		case S_MODE_VI:
			if (vi_s_init(sp, ep))
				goto err1;
			break;
		default:
			abort();
		}

		/* While the screen stays the same, edit files. */
		mode = sp->flags & (S_MODE_EX | S_MODE_VI);
		do {
			switch(sp->flags & (S_MODE_EX | S_MODE_VI)) {
			case S_MODE_EX:
				if (ex(sp, ep))
					F_SET(sp, S_EXIT_FORCE);
				break;
			case S_MODE_VI:
				if (vi(sp, ep))
					F_SET(sp, S_EXIT_FORCE);
				break;
			default:
				abort();
			}

			switch (sp->flags & S_FILE_CHANGED) {
			case S_EXIT:
				if (file_stop(sp, ep, 0))
					F_CLR(sp, S_EXIT);
				break;
			case S_EXIT_FORCE:
				if (file_stop(sp, ep, 1))
					F_CLR(sp, S_EXIT_FORCE);
				break;
			case S_SWITCH_FORCE:
				F_CLR(sp, S_SWITCH_FORCE);
				if (file_start(sp, sp->enext) != NULL)
					if (file_stop(sp, ep, 1))
						(void)file_stop(sp,
						    sp->enext, 1);
					else {
						sp->eprev = ep;
						ep = sp->enext;
					}
				break;
			case S_SWITCH:
				F_CLR(sp, S_SWITCH);
				if (file_start(sp, sp->enext) != NULL)
					if (file_stop(sp, ep, 0))
						(void)file_stop(sp,
						    sp->enext, 1);
					else {
						sp->eprev = ep;
						ep = sp->enext;
					}
				break;
			}
		} while (!F_ISSET(sp, S_EXIT | S_EXIT_FORCE) &&
		    mode == sp->flags & (S_MODE_EX | S_MODE_VI));

		/* End the screen. */
		switch(mode) {
		case S_MODE_EX:
			if (ex_s_end(sp))
				goto err1;
			break;
		case S_MODE_VI:
			if (vi_s_end(sp))
				goto err1;
			break;
		default:
			abort();
		}
	} while (!F_ISSET(sp, S_EXIT | S_EXIT_FORCE));

	/* Yeah, I don't like it either. */
	if (0)
err1:		eval = 1;

	/* Reset anything that needs resetting. */
err2:	reset(sp);

	/* Flush any left-over error messages. */
	ex_s_msgflush(sp);

	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &gp->original_termios))
		err("tcsetattr: %s", strerror(errno));
	exit(eval);
}

/*
 * reset --
 *	Reset any changed global state.
 */
static void
reset(sp)
	SCR *sp;
{
	char *tty;

	if (sp->gp->flags & G_SETMODE) {			/* O_MESG */
		if ((tty = ttyname(STDERR_FILENO)) == NULL)
			msgq(sp, M_ERROR, "ttyname: %s", strerror(errno));
		else if (chmod(tty, sp->gp->origmode) < 0)
			msgq(sp, M_ERROR, "%s: %s", strerror(errno));
	}
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
					err("%s: %s", myname, strerror(errno));
				memmove(argv[0], "-c$", 4);
			} else if (argv[0][1] == '/') {
				p = argv[0];
				len = strlen(argv[0]);
				if ((argv[0] = malloc(len + 3)) == NULL)
					err("%s: %s", myname, strerror(errno));
				argv[0][0] = '-';
				argv[0][1] = 'c';
				(void)strcpy(argv[0] + 2, p + 1);
			} else if (isdigit(argv[0][1])) {
				p = argv[0];
				len = strlen(argv[0]);
				if ((argv[0] = malloc(len + 3)) == NULL)
					err("%s: %s", myname, strerror(errno));
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

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
}
