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
static char sccsid[] = "$Id: main.c,v 5.27 1992/06/07 14:00:31 bostic Exp $ (Berkeley) $Date: 1992/06/07 14:00:31 $";
#endif /* not lint */

#include <sys/param.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vi.h"
#include "exf.h"
#include "excmd.h"
#include "options.h"
#include "pathnames.h"
#include "seq.h"
#include "tag.h"
#include "extern.h"

#ifdef DEBUG
FILE *tracefp;
#endif

static jmp_buf jmpenv;
int reading_exrc;

static void obsolete __P((char *[]));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	EXCMDARG cmd;
	int ch, i;
	char *excmdarg, *err, *p, *tag, path[MAXPATHLEN];

	/* Set mode based on the program name. */
	if ((p = rindex(*argv, '/')) == NULL)
		p = *argv;
	else
		++p;
	if (!strcmp(p, "ex"))
		mode = MODE_EX;
	else if (!strcmp(p, "view")) {
		SET(O_READONLY)
		mode = MODE_VI;
	} else
		mode = MODE_VI;

	obsolete(argv);
	excmdarg = err = tag = NULL;
	while ((ch = getopt(argc, argv, "c:emRrT:t:v")) != EOF)
		switch(ch) {
		case 'c':		/* Run the command. */
			excmdarg = optarg;
			break;
		case 'e':		/* Ex mode. */
			mode = MODE_EX;
			break;
#ifndef NO_ERRLIST
		case 'm':		/* Error list. */
			err = optarg;
			break;
#endif
		case 'R':		/* Readonly. */
			SET(O_READONLY);
			break;
		case 'r':		/* Recover. */
			(void)fprintf(stderr,
			    "%s: recover option not currently implemented.\n",
			    *argv);
			exit(1);
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((tracefp = fopen(optarg, "w")) == NULL)
				(void)fprintf(stderr,
				    "%s: %s", optarg, strerror(errno));
			(void)fprintf(tracefp,
			    "\n===\ntrace: open %s\n", optarg);
			break;
#endif
		case 't':		/* Tag. */
			tag = optarg;
			break;
		case 'v':		/* Vi mode. */
			mode = MODE_VI;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Initialize the key sequence list. */
	seq_init();

	/* Initialize the options. */
	if (opts_init()) {
		msg_eflush();
		exit(1);
	}

#ifndef NO_DIGRAPH
	digraph_init();
#endif

	/* Initialize special key table. */
	gb_init();

	/* Initialize file list. */
	file_init();

	/* Any remaining arguments are file names. */
	if (argc)
		file_set(argc, argv);

	/* Use an error list file if specified. */
#ifndef NO_ERRLIST
	if (err) {
		SETCMDARG(cmd, C_ERRLIST, 0, OOBLNO, 0, 0, err);
		ex_errlist(&cmd);
	} else
#endif
	/* Use a tag file if specified. */
	if (tag) {
		SETCMDARG(cmd, C_TAG, 0, OOBLNO, 0, 0, tag);
		ex_tag(&cmd);
	} else if (file_start(file_first())) {
		msg_eflush();
		exit(1);
	}

	/*
	 * Source the system, ~user and local .exrc files.
	 * XXX
	 * Check the correct order for these.
	 */
	reading_exrc = 1;
	(void)ex_cfile(_PATH_SYSEXRC, 0);
	if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/.exrc", p);
		(void)ex_cfile(path, 0);
	}
	if (ISSET(O_EXRC))
		(void)ex_cfile(_PATH_EXRC, 0);
	reading_exrc = 0;

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL)
			msg("Error: %s", strerror(errno));
		else {
			(void)ex_cstring(p, strlen(p), 1);
			free(p);
		}

	/* Do any commands from the command line. */
	if (excmdarg)
		(void)ex_cstring(excmdarg, strlen(excmdarg), 0);

	/* Catch HUP, INT, WINCH */
	(void)signal(SIGHUP, onhup);
	(void)signal(SIGINT, SIG_IGN);		/* XXX */
	(void)signal(SIGWINCH, onwinch);

	/* Repeatedly call ex() or vi() until the mode is set to MODE_QUIT. */
	while (mode != MODE_QUIT) {
#ifdef NOT_RIGHT_NOW
		/* Maybe we just aborted a change? */
		if (setjmp(jmpenv))
			abortdo();
#endif

		(void)signal(SIGINT, trapint);
		switch (mode) {
		case MODE_VI:
			vi();
			break;
		case MODE_EX:
			ex();
			break;
		}
	}

	/* Flush any left-over error message. */
	msg_eflush();

	exit(0);
	/* NOTREACHED */
}

/* ARGSUSED */
void
trapint(signo)
	int signo;
{
#ifdef NOT_RIGHT_NOW
	abortdo();
#endif
	doingglobal = 0;
	longjmp(jmpenv, 1);
}

static void
obsolete(argv)
	char *argv[];
{
	static char *eofarg = "-c$";

	/*
	 * Translate old style arguments into something getopt will like.
	 * Change "+/command" into "-ccommand".
	 * Change "+" into "-c$".
	 */
	while (*++argv)
		if (argv[0][0] == '+')
			if (argv[0][1] == '\0')
				argv[0] = eofarg;
			else if (argv[0][1] == '/') {
				argv[0][0] = '-';
				argv[0][1] = 'c';
			}
			
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: vi [-eRrv] [-c command] [-m file] [-t tag]\n");
	exit(1);
}
