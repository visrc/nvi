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
static char sccsid[] = "$Id: main.c,v 5.54 1993/03/01 12:44:26 bostic Exp $ (Berkeley) $Date: 1993/03/01 12:44:26 $";
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
#include "screen.h"
#include "seq.h"
#include "term.h"

#ifdef DEBUG
FILE *tracefp;
#endif

struct termios original_termios;	/* Original terminal state. */

char *VB;				/* Visual bell termcap string. */

static void eflush __P((EXF *));
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
	EXF *ep, *tep, fake_exf;
	SCR fake_scr;
	int ch;
	char *excmdarg, *errf, *p, *tag, path[MAXPATHLEN];

	/* Stop if indirect through NULL. */
	if (reenter++)
		abort();

	/*
	 * Fake up a file structure so it's available for the ex/vi routines.
	 * Do basic initialization so there's a place to hang error messages
	 * and such.
	 *
	 * XXX
	 * It's going to be difficult to verify that all of the necessary
	 * fields are filled in (or that F_DUMMY is checked appropriately).
	 */
	ep = &fake_exf;
	exf_def(ep);
	ep->flags = F_DUMMY | F_IGNORE;
	ep->msg = ex_msg;
	SCRP(ep) = &fake_scr;
	scr_def(SCRP(ep));
	set_window_size(ep, 0);

	/* Set mode based on the program name. */
	if ((p = strrchr(*argv, '/')) == NULL)
		p = *argv;
	else
		++p;
	if (!strcmp(p, "ex") || !strcmp(p, "nex"))
		FF_SET(ep, F_MODE_EX);
	else if (!strcmp(p, "view")) {
		SET(O_READONLY)
		FF_SET(ep, F_MODE_VI);
	} else
		FF_SET(ep, F_MODE_VI);

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
			FF_SET(ep, F_MODE_EX);
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
			FF_SET(ep, F_MODE_VI);
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
	if (opts_init(ep)) {
		eflush(ep);
		exit(1);
	}

#ifndef NO_DIGRAPH
	digraph_init(ep);
#endif

	/* Initialize special key table. */
	gb_init(ep);

	/* Initialize file list. */
	file_init();

	/*
	 * Source the system, ~user and local .exrc files.
	 * XXX
	 * Check the correct order for these.
	 */
	(void)ex_cfile(ep, _PATH_SYSEXRC, 0);
	if ((p = getenv("HOME")) != NULL && *p) {
		(void)snprintf(path, sizeof(path), "%s/.exrc", p);
		(void)ex_cfile(ep, path, 0);
	}
	if (ISSET(O_EXRC))
		(void)ex_cfile(ep, _PATH_EXRC, 0);

	/* Source the EXINIT environment variable. */
	if ((p = getenv("EXINIT")) != NULL)
		if ((p = strdup(p)) == NULL)
			ep->msg(ep, M_ERROR, "Error: %s", strerror(errno));
		else {
			(void)ex_cstring(ep, (u_char *)p, strlen(p), 1);
			free(p);
		}

	/* Any remaining arguments are file names. */
	if (argc)
		file_set(argc, argv);

	/* Use an error list file if specified. */
#ifndef NO_ERRLIST
	if (errf) {
		SETCMDARG(cmd, C_ERRLIST, 0, OOBLNO, 0, 0, errf);
		if (ex_errlist(ep, &cmd)) {
			eflush(ep);
			exit(1);
		}
	} else
#endif
	/* Use a tag file if specified. */
	if (tag) {
		SETCMDARG(cmd, C_TAG, 0, OOBLNO, 0, 0, tag);
		if (ex_tagpush(ep, &cmd)) {
			eflush(ep);
			exit(1);
		}
	} else
		ep->enext = file_first(1);

	/* Get a real EXF structure. */
	if ((tep = file_switch(ep, 0)) == NULL) {
		eflush(ep);
		exit(1);
	}
	ep = tep;

	/* Can now handle signals. */
	(void)signal(SIGHUP, onhup);
	(void)signal(SIGINT, SIG_IGN);		/* XXX */

	/* Do command line commands. */
	if (excmdarg != NULL)
		(void)ex_cstring(ep, (u_char *)excmdarg, strlen(excmdarg), 0);

	while (!FF_ISSET(ep, F_EXIT | F_EXIT_FORCE)) {

#define	MFLAGS	(F_MODE_EX | F_MODE_VI)
		switch(ep->flags & MFLAGS) {
		case F_MODE_EX:
			if (ex(ep))
				FF_SET(ep, F_EXIT_FORCE);
			break;
		case F_MODE_VI:
			if (vi(ep))
				FF_SET(ep, F_EXIT_FORCE);
			break;
		default:
			abort();
		}

#define	EFLAGS	(F_EXIT | F_EXIT_FORCE | F_SWITCH | F_SWITCH_FORCE)
		switch (ep->flags & EFLAGS) {
		case F_EXIT:
			if (file_stop(ep, 0)) {
				FF_CLR(ep, F_EXIT);
				break;
			}
			break;
		case F_EXIT_FORCE:
			(void)file_stop(ep, 1);
			break;
		case F_SWITCH_FORCE:
			if ((tep = file_switch(ep, 1)) != NULL)
				ep = tep;
			FF_CLR(ep, F_SWITCH_FORCE);
			break;
		case F_SWITCH:
			if ((tep = file_switch(ep, 0)) != NULL)
				ep = tep;
			FF_CLR(ep, F_SWITCH);
			break;
		}
	}

	/* Reset anything that needs resetting. */
	opts_end(ep);

	/* Flush any left-over error message. */
	eflush(ep);

	exit(0);
}

static void
obsolete(argv)
	char *argv[];
{
	size_t len;
	char *p, *pname;

	/*
	 * Translate old style arguments into something getopt will like.
	 * Make sure it's not text space memory, because ex changes the
	 * strings.
	 *	Change "+/command" into "-ccommand".
	 *	Change "+" into "-c$".
	 *	Change "+[0-9]*" into "-c:[0-9]".
	 */
	for (pname = argv[0]; *++argv;)
		if (argv[0][0] == '+')
			if (argv[0][1] == '\0') {
				if ((argv[0] = malloc(4)) == NULL)
					err("%s: %s", pname, strerror(errno));
				memmove(argv[0], "-c$", 4);
			} else if (argv[0][1] == '/') {
				p = argv[0];
				len = strlen(argv[0]);
				if ((argv[0] = malloc(len + 3)) == NULL)
					err("%s: %s", pname, strerror(errno));
				argv[0][0] = '-';
				argv[0][1] = 'c';
				(void)strcpy(argv[0] + 2, p + 1);
			} else if (isdigit(argv[0][1])) {
				p = argv[0];
				len = strlen(argv[0]);
				if ((argv[0] = malloc(len + 3)) == NULL)
					err("%s: %s", pname, strerror(errno));
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

/*
 * eflush --
 *	Flush any accumulated messages to the terminal.
 */
static void
eflush(ep)
	EXF *ep;
{
	MSG *mp;

	if (FF_ISSET(ep, F_BELLSCHED))
		bell(ep);
	for (mp = ep->msgp;
	    mp != NULL && !(mp->flags & M_EMPTY); mp = mp->next) {
		(void)fprintf(ep->stdfp, "%.*s\n", mp->len, mp->mbuf);
		mp->flags |= M_EMPTY;
	}
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
