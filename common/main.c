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
static char sccsid[] = "$Id: main.c,v 5.46 1993/02/19 14:45:02 bostic Exp $ (Berkeley) $Date: 1993/02/19 14:45:02 $";
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
#include "tag.h"
#include "term.h"

#ifdef DEBUG
FILE *tracefp;
#endif

enum editmode mode;			/* Editor mode; see vi.h. */

struct termios original_termios;	/* Original terminal state. */

char *VB;				/* Visual bell termcap string. */

static void err __P((const char *, ...));
static void obsolete __P((char *[]));
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
	EXF *ep, fake_exf;
	int ch;
	char *excmdarg, *errf, *p, *tag, path[MAXPATHLEN];

	/* Stop if indirect through NULL. */
	if (reenter++)
		abort();

	/* Set mode based on the program name. */
	if ((p = strrchr(*argv, '/')) == NULL)
		p = *argv;
	else
		++p;
	if (!strcmp(p, "ex") || !strcmp(p, "nex"))
		mode = MODE_EX;
	else if (!strcmp(p, "view")) {
		SET(O_READONLY)
		mode = MODE_VI;
	} else
		mode = MODE_VI;

	/* Get original terminal information. */
	if (tcgetattr(STDIN_FILENO, &original_termios))
		err("%s: tcgetttr: %s\n", p, strerror(errno));
		
	obsolete(argv);

	excmdarg = errf = tag = NULL;
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
			errf = optarg;
			break;
#endif
		case 'R':		/* Readonly. */
			SET(O_READONLY);
			break;
		case 'r':		/* Recover. */
			err("%s: recover option not yet implemented", p);
#ifdef DEBUG
		case 'T':		/* Trace. */
			if ((tracefp = fopen(optarg, "w")) == NULL)
				err("%s: %s: %s", p, optarg, strerror(errno));
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
	if (opts_init(NULL)) {
		msg_eflush(NULL);
		exit(1);
	}

#ifndef NO_DIGRAPH
	digraph_init(NULL);
#endif

	/* Initialize special key table. */
	gb_init(NULL);

	/*
	 * Source the system, ~user and local .exrc files.
	 * XXX
	 * Check the correct order for these.
	 */
	(void)ex_cfile(NULL, _PATH_SYSEXRC, 0);
	if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/.exrc", p);
		(void)ex_cfile(NULL, path, 0);
	}
	if (ISSET(O_EXRC))
		(void)ex_cfile(NULL, _PATH_EXRC, 0);

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL)
			msg(NULL, M_ERROR, "Error: %s", strerror(errno));
		else {
			(void)ex_cstring(NULL, (u_char *)p, strlen(p), 1);
			free(p);
		}

	/* Initialize file list. */
	file_init();

	/* Any remaining arguments are file names. */
	if (argc)
		file_set(argc, argv);

	/*
	 * Fake up a file structure, so it's available when we call
	 * ex/vi routines.  Do basic initialization, so error messages
	 * get printed.
	 */
	 ep = &fake_exf;
	 file_def(ep);
	 ep->cols = 80;
	 ep->flags = F_IGNORE;

	/* Use an error list file if specified. */
#ifndef NO_ERRLIST
	if (errf) {
		SETCMDARG(cmd, C_ERRLIST, 0, OOBLNO, 0, 0, errf);
		ex_errlist(ep, &cmd);
	} else
#endif
	/* Use a tag file if specified. */
	if (tag) {
		SETCMDARG(cmd, C_TAG, 0, OOBLNO, 0, 0, tag);
		if (ex_tagpush(ep, &cmd)) {
			msg_eflush(ep);
			exit(1);
		}
	} else if (file_start(file_first(1))) {
		msg_eflush(ep);
		exit(1);
	}

	/* Should have a real EXF structure by now. */
	ep = curf;

	/* Do any commands from the command line. */
	if (excmdarg)
		(void)ex_cstring(ep, (u_char *)excmdarg, strlen(excmdarg), 0);

	/* Catch HUP, INT, WINCH */
	(void)signal(SIGHUP, onhup);
	(void)signal(SIGINT, SIG_IGN);		/* XXX */
	(void)signal(SIGWINCH, onwinch);

	/* Repeatedly call ex() or vi() until the mode is set to MODE_QUIT. */
	while (mode != MODE_QUIT) {
		switch (mode) {
		case MODE_VI:
			if (vi(ep))
				mode = MODE_QUIT;
			break;
		case MODE_EX:
			if (ex(ep))
				mode = MODE_QUIT;
			break;
		default:
			abort();
		}
		/*
		 * XXX
		 * THE UNDERLYING EXF MAY HAVE CHANGED.
		 */
		ep = curf;
	}

	/* Reset anything that needs resetting. */
	opts_end(ep);

	/* Flush any left-over error message. */
	msg_eflush(ep);

	exit(0);
	/* NOTREACHED */
}

static void
obsolete(argv)
	char *argv[];
{
	char *pname;

	/*
	 * Translate old style arguments into something getopt will like.
	 * Make sure it's not text space memory, because ex changes the
	 * strings.
	 *	Change "+/command" into "-ccommand".
	 *	Change "+" into "-c$".
	 */
	for (pname = argv[0]; *++argv;)
		if (argv[0][0] == '+')
			if (argv[0][1] == '\0') {
				if ((argv[0] = malloc(4)) == NULL)
					err("%s: %s", pname, strerror(errno));
				memmove(argv[0], "-c$", 4);
			} else if (argv[0][1] == '/') {
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
